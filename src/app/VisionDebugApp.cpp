#include "app/VisionDebugApp.hpp"

#include "network/UdpTelemetryReceiver.hpp"
#include "overlay/IntersectionOverlay.hpp"
#include "overlay/LineOverlay.hpp"
#include "overlay/MarkerOverlay.hpp"
#include "protocol/TelemetryMessage.hpp"
#include "telemetry/GridMapTracker.hpp"
#include "telemetry/MarkerTracker.hpp"
#include "telemetry/TelemetryStore.hpp"
#include "telemetry/VisionLogFormatter.hpp"
#include "ui/VisionLogWindow.hpp"
#include "ui/VideoWindow.hpp"
#include "video/GcsDiscoveryBeacon.hpp"
#include "video/UdpMjpegReceiver.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>

namespace gcs::app {
namespace {

class TelemetryThread {
public:
    bool start(
        std::uint16_t port,
        int timeout_ms,
        telemetry::TelemetryStore& store,
        telemetry::GridMapTracker& grid_map,
        telemetry::MarkerTracker& marker_tracker)
    {
        if (!receiver_.open(port)) {
            last_error_ = receiver_.lastError();
            return false;
        }

        running_ = true;
        worker_ = std::thread([this, timeout_ms, &store, &grid_map, &marker_tracker]() {
            while (running_) {
                std::string payload;
                if (!receiver_.receive(payload, std::clamp(timeout_ms, 1, 100))) {
                    if (receiver_.lastError() != "timeout") {
                        std::lock_guard<std::mutex> lock(error_mutex_);
                        last_error_ = receiver_.lastError();
                    }
                    continue;
                }

                const auto parsed = protocol::parseTelemetryJson(payload);
                if (!parsed) {
                    std::lock_guard<std::mutex> lock(error_mutex_);
                    last_error_ = "dropped malformed telemetry JSON";
                    continue;
                }

                {
                    std::lock_guard<std::mutex> lock(stats_mutex_);
                    stats_.observe(*parsed);
                }
                store.observe(*parsed);
                grid_map.observe(parsed->vision.grid_node);
                // Cycle 13: drive the heading arrow's sub-cell position from
                // the drone's fractional progress since the last committed node.
                grid_map.observeDronePosition(
                    parsed->vision.drone_position.valid,
                    parsed->vision.drone_position.grid_offset_x,
                    parsed->vision.drone_position.grid_offset_y);
                // Cycle 23: feed discovered-marker registry into MarkerTracker
                // so the new side panel renders the id-sorted list.
                marker_tracker.observe(parsed->mission);
            }
        });
        return true;
    }

    void stop()
    {
        running_ = false;
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    protocol::TelemetryStats stats() const
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return stats_;
    }

    std::string takeLastError()
    {
        std::lock_guard<std::mutex> lock(error_mutex_);
        std::string output = std::move(last_error_);
        last_error_.clear();
        return output;
    }

private:
    network::UdpTelemetryReceiver receiver_;
    std::atomic<bool> running_ {false};
    std::thread worker_;
    mutable std::mutex stats_mutex_;
    protocol::TelemetryStats stats_;
    std::mutex error_mutex_;
    std::string last_error_;
};

class VideoReceiveThread {
public:
    bool start(std::uint16_t port, int timeout_ms)
    {
        if (!receiver_.open(port)) {
            last_error_ = receiver_.lastError();
            return false;
        }

        running_ = true;
        worker_ = std::thread([this, timeout_ms]() {
            const int poll_timeout_ms = std::clamp(timeout_ms, 1, 50);
            while (running_) {
                auto frame = receiver_.receiveFrame(poll_timeout_ms);
                {
                    std::lock_guard<std::mutex> lock(stats_mutex_);
                    stats_ = receiver_.stats();
                }
                if (frame) {
                    std::lock_guard<std::mutex> lock(frame_mutex_);
                    if (latest_frame_) {
                        ++overwritten_frames_;
                    }
                    latest_frame_ = std::move(frame);
                    continue;
                }

                if (receiver_.lastError() != "timeout") {
                    std::lock_guard<std::mutex> lock(error_mutex_);
                    last_error_ = receiver_.lastError();
                }
            }
        });
        return true;
    }

    void stop()
    {
        running_ = false;
        if (worker_.joinable()) {
            worker_.join();
        }
        receiver_.close();
    }

    std::optional<video::JpegFrame> takeLatestFrame()
    {
        std::lock_guard<std::mutex> lock(frame_mutex_);
        if (!latest_frame_) {
            return std::nullopt;
        }
        auto output = std::move(latest_frame_);
        latest_frame_.reset();
        return output;
    }

    video::UdpMjpegReceiverStats stats() const
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return stats_;
    }

    std::uint64_t overwrittenFrames() const
    {
        std::lock_guard<std::mutex> lock(frame_mutex_);
        return overwritten_frames_;
    }

    std::string takeLastError()
    {
        std::lock_guard<std::mutex> lock(error_mutex_);
        std::string output = std::move(last_error_);
        last_error_.clear();
        return output;
    }

private:
    video::UdpMjpegReceiver receiver_;
    std::atomic<bool> running_ {false};
    std::thread worker_;
    mutable std::mutex frame_mutex_;
    std::optional<video::JpegFrame> latest_frame_;
    std::uint64_t overwritten_frames_ = 0;
    mutable std::mutex stats_mutex_;
    video::UdpMjpegReceiverStats stats_;
    mutable std::mutex error_mutex_;
    std::string last_error_;
};

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

int VisionDebugApp::run(const VisionDebugOptions& options)
{
    VideoReceiveThread video_thread;
    if (!video_thread.start(options.video_port, options.video_timeout_ms)) {
        std::cerr << "failed to open UDP video receiver on port "
                  << options.video_port << ": " << video_thread.takeLastError() << "\n";
        return 1;
    }

    telemetry::TelemetryStore telemetry_store;
    telemetry::GridMapTracker grid_map_tracker;
    telemetry::MarkerTracker marker_tracker;
    TelemetryThread telemetry_thread;
    if (!telemetry_thread.start(
            options.telemetry_port,
            options.telemetry_timeout_ms,
            telemetry_store,
            grid_map_tracker,
            marker_tracker)) {
        std::cerr << "failed to open UDP telemetry receiver on port "
                  << options.telemetry_port << ": " << telemetry_thread.takeLastError() << "\n";
        video_thread.stop();
        return 1;
    }

    video::GcsDiscoveryBeacon beacon;
    beacon.start(options.video_port);

    ui::VideoWindow window(options.title);
    ui::VisionLogWindow log_window("Astroquad Vision Log");
    window.showStatus("waiting for video stream...");

    std::cout << "uav_gcs_vision_debug\n"
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
        const auto frame = video_thread.takeLatestFrame();
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

        const std::string video_error = video_thread.takeLastError();
        if (!video_error.empty()) {
            std::cerr << "video receive warning: " << video_error << "\n";
        }

        const std::string telemetry_error = telemetry_thread.takeLastError();
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
                    *latest, marker_tracker.latestMission(), telemetry_thread.stats());
                detail += formatVideoStatsLine(
                    video_thread.stats(),
                    video_thread.overwrittenFrames(),
                    display_fps);
                if (!log_window.update(grid_text, markers_text, detail)) {
                    std::cout << grid_text << markers_text << detail;
                }
            } else {
                const auto stats = telemetry_thread.stats();
                std::string detail =
                    "=== Network ===\n"
                    "[vision] no telemetry packets yet packets=" +
                    std::to_string(stats.received_packets) +
                    " dropped=" + std::to_string(stats.dropped_packets) + "\n";
                detail += formatVideoStatsLine(
                    video_thread.stats(),
                    video_thread.overwrittenFrames(),
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

    video_thread.stop();
    telemetry_thread.stop();
    return 0;
}

} // namespace gcs::app
