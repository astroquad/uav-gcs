#include "app/AstroquadGcsApp.hpp"

#include "app/TelemetryWorker.hpp"
#include "app/VideoReceiveWorker.hpp"
#include "overlay/IntersectionOverlay.hpp"
#include "overlay/LineOverlay.hpp"
#include "overlay/MarkerOverlay.hpp"
#include "telemetry/GridMapTracker.hpp"
#include "telemetry/MarkerTracker.hpp"
#include "telemetry/TelemetryStore.hpp"
#include "telemetry/VisionLogFormatter.hpp"
#include "ui/VisionLogWindow.hpp"
#include "ui/VideoWindow.hpp"
#include "video/GcsDiscoveryBeacon.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace gcs::app {
namespace {

std::string formatVideoStatsLine(
    const video::UdpMjpegReceiverStats& stats,
    std::uint64_t overwritten_frames,
    double display_fps)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1)
           << "[video-rx] display_fps=" << display_fps
           << " packets=" << stats.packets_received
           << " completed=" << stats.completed_frames
           << " incomplete=" << stats.incomplete_frames
           << " malformed=" << stats.malformed_packets
           << " old_packets=" << stats.old_packets
           << " mismatch_resets=" << stats.chunk_mismatch_resets
           << " latest_overwritten=" << overwritten_frames
           << " last_chunks=" << stats.last_chunk_count
           << " last_bytes=" << stats.last_frame_bytes << "\n";
    return stream.str();
}

} // namespace

int AstroquadGcsApp::run(const AstroquadGcsOptions& options)
{
    VideoReceiveWorker video_worker;
    if (!video_worker.start(options.video_port, options.video_timeout_ms)) {
        std::cerr << "failed to open UDP video receiver on port "
                  << options.video_port << ": " << video_worker.takeLastError() << "\n";
        return 1;
    }

    telemetry::TelemetryStore telemetry_store;
    telemetry::GridMapTracker grid_map_tracker;
    telemetry::MarkerTracker marker_tracker;
    TelemetryWorker telemetry_worker;
    if (!telemetry_worker.start(
            options.telemetry_port,
            options.telemetry_timeout_ms,
            telemetry_store,
            grid_map_tracker,
            marker_tracker)) {
        std::cerr << "failed to open UDP telemetry receiver on port "
                  << options.telemetry_port << ": " << telemetry_worker.takeLastError() << "\n";
        video_worker.stop();
        return 1;
    }

    video::GcsDiscoveryBeacon beacon;
    beacon.start(options.video_port);

    ui::VideoWindow window(options.title);
    ui::VisionLogWindow log_window("Astroquad GCS Log");
    window.showStatus("waiting for video stream...");

    std::cout << "astroquad-gcs\n"
              << "  video UDP port: " << options.video_port << "\n"
              << "  telemetry UDP port: " << options.telemetry_port << "\n"
              << "  video_timeout_ms: " << options.video_timeout_ms << "\n"
              << "  vision_log_interval_ms: " << options.marker_log_interval_ms << "\n"
              << "  press q or ESC in the video window to exit\n";

    auto last_frame_time = std::chrono::steady_clock::now();
    auto last_log_time = std::chrono::steady_clock::now();
    auto fps_window_time = std::chrono::steady_clock::now();
    int displayed_frames_in_window = 0;
    double display_fps = 0.0;
    bool received_any_frame = false;

    while (true) {
        const auto frame = video_worker.takeLatestFrame();
        if (frame) {
            last_frame_time = std::chrono::steady_clock::now();
            received_any_frame = true;

            const auto marker_frame = telemetry_store.findForFrame(
                frame->frame_id,
                frame->timestamp_ms);
            std::vector<overlay::OverlayPrimitive> overlays;
            if (marker_frame) {
                auto line_overlays = overlay::buildLineOverlays(
                    marker_frame->line,
                    marker_frame->width,
                    marker_frame->height);
                overlays.insert(overlays.end(), line_overlays.begin(), line_overlays.end());
                auto intersection_overlays =
                    overlay::buildIntersectionOverlays(
                        marker_frame->intersection,
                        marker_frame->intersection_decision,
                        marker_frame->width,
                        marker_frame->height);
                overlays.insert(
                    overlays.end(),
                    intersection_overlays.begin(),
                    intersection_overlays.end());
                auto marker_overlays = overlay::buildMarkerOverlays(marker_frame->markers);
                overlays.insert(overlays.end(), marker_overlays.begin(), marker_overlays.end());
            }

            if (!window.showFrame(*frame, overlays)) {
                std::cerr << "video display warning: failed to decode JPEG frame\n";
            } else {
                ++displayed_frames_in_window;
            }
        } else {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - last_frame_time);
            if (!received_any_frame && elapsed.count() >= options.video_timeout_ms) {
                window.showStatus("waiting for video stream...");
                last_frame_time = std::chrono::steady_clock::now();
            }
        }

        const std::string video_error = video_worker.takeLastError();
        if (!video_error.empty()) {
            std::cerr << "video receive warning: " << video_error << "\n";
        }

        const std::string telemetry_error = telemetry_worker.takeLastError();
        if (!telemetry_error.empty()) {
            std::cerr << "telemetry receive warning: " << telemetry_error << "\n";
        }

        const auto now = std::chrono::steady_clock::now();
        const auto fps_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - fps_window_time);
        if (fps_elapsed.count() >= 1000) {
            display_fps = displayed_frames_in_window * 1000.0 /
                std::max(1, static_cast<int>(fps_elapsed.count()));
            displayed_frames_in_window = 0;
            fps_window_time = now;
        }

        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_log_time).count()
            >= options.marker_log_interval_ms) {
            const auto grid_text = grid_map_tracker.render();
            const auto markers_text = marker_tracker.render();
            if (const auto latest = telemetry_store.latest()) {
                // Cycle 23: mission-aware overload so the detail panel leads
                // with the "=== Mission ===" section before the per-frame
                // vision dump.
                auto detail = telemetry::formatVisionLog(
                    *latest, marker_tracker.latestMission(), telemetry_worker.stats());
                detail += formatVideoStatsLine(
                    video_worker.stats(),
                    video_worker.overwrittenFrames(),
                    display_fps);
                if (!log_window.update(grid_text, markers_text, detail)) {
                    std::cout << grid_text << markers_text << detail;
                }
            } else {
                const auto stats = telemetry_worker.stats();
                std::string detail =
                    "=== Network ===\n"
                    "[vision] no telemetry packets yet packets=" +
                    std::to_string(stats.received_packets) +
                    " dropped=" + std::to_string(stats.dropped_packets) + "\n";
                detail += formatVideoStatsLine(
                    video_worker.stats(),
                    video_worker.overwrittenFrames(),
                    display_fps);
                if (!log_window.update(grid_text, markers_text, detail)) {
                    std::cout << grid_text << markers_text << detail;
                }
            }
            last_log_time = now;
        }

        log_window.poll();
        if (window.shouldClose(1)) {
            break;
        }
    }

    video_worker.stop();
    telemetry_worker.stop();
    return 0;
}

} // namespace gcs::app
