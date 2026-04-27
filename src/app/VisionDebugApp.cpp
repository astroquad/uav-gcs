#include "app/VisionDebugApp.hpp"

#include "network/UdpTelemetryReceiver.hpp"
#include "overlay/LineOverlay.hpp"
#include "overlay/MarkerOverlay.hpp"
#include "protocol/TelemetryMessage.hpp"
#include "telemetry/TelemetryStore.hpp"
#include "telemetry/VisionLogFormatter.hpp"
#include "ui/VisionLogWindow.hpp"
#include "ui/VideoWindow.hpp"
#include "video/GcsDiscoveryBeacon.hpp"
#include "video/UdpMjpegReceiver.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace gcs::app {
namespace {

std::int64_t unixTimestampMs()
{
    const auto now = std::chrono::system_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch());
    return ms.count();
}

class TelemetryThread {
public:
    bool start(
        std::uint16_t port,
        int timeout_ms,
        telemetry::TelemetryStore& store)
    {
        if (!receiver_.open(port)) {
            last_error_ = receiver_.lastError();
            return false;
        }

        running_ = true;
        worker_ = std::thread([this, timeout_ms, &store]() {
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

} // namespace

int VisionDebugApp::run(const VisionDebugOptions& options)
{
    video::UdpMjpegReceiver video_receiver;
    if (!video_receiver.open(options.video_port)) {
        std::cerr << "failed to open UDP video receiver on port "
                  << options.video_port << ": " << video_receiver.lastError() << "\n";
        return 1;
    }

    telemetry::TelemetryStore telemetry_store;
    TelemetryThread telemetry_thread;
    if (!telemetry_thread.start(
            options.telemetry_port,
            options.telemetry_timeout_ms,
            telemetry_store)) {
        std::cerr << "failed to open UDP telemetry receiver on port "
                  << options.telemetry_port << ": " << telemetry_thread.takeLastError() << "\n";
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
    bool received_any_frame = false;

    while (true) {
        const int poll_timeout_ms = std::clamp(options.video_timeout_ms, 1, 50);
        const auto frame = video_receiver.receiveFrame(poll_timeout_ms);
        if (frame) {
            last_frame_time = std::chrono::steady_clock::now();
            received_any_frame = true;

            const auto marker_frame = telemetry_store.findForFrame(
                frame->frame_id,
                frame->timestamp_ms);
            std::vector<overlay::OverlayPrimitive> overlays;
            if (marker_frame) {
                auto line_overlays = overlay::buildLineOverlays(marker_frame->line);
                overlays.insert(overlays.end(), line_overlays.begin(), line_overlays.end());
                auto marker_overlays = overlay::buildMarkerOverlays(marker_frame->markers);
                overlays.insert(overlays.end(), marker_overlays.begin(), marker_overlays.end());
            }

            if (!window.showFrame(*frame, overlays)) {
                std::cerr << "video display warning: failed to decode JPEG frame\n";
            }
        } else if (video_receiver.lastError() == "timeout") {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - last_frame_time);
            if (!received_any_frame && elapsed.count() >= options.video_timeout_ms) {
                window.showStatus("waiting for video stream...");
                last_frame_time = std::chrono::steady_clock::now();
            }
        } else {
            std::cerr << "video receive warning: " << video_receiver.lastError() << "\n";
        }

        const std::string telemetry_error = telemetry_thread.takeLastError();
        if (!telemetry_error.empty()) {
            std::cerr << "telemetry receive warning: " << telemetry_error << "\n";
        }

        const auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_log_time).count()
            >= options.marker_log_interval_ms) {
            if (const auto latest = telemetry_store.latest()) {
                const auto text = telemetry::formatVisionLog(
                    *latest,
                    telemetry_thread.stats(),
                    unixTimestampMs());
                if (!log_window.update(text)) {
                    std::cout << text;
                }
            } else {
                const auto stats = telemetry_thread.stats();
                const std::string text =
                    "[vision] no telemetry packets yet packets=" +
                    std::to_string(stats.received_packets) +
                    " dropped=" + std::to_string(stats.dropped_packets) + "\n";
                if (!log_window.update(text)) {
                    std::cout << text;
                }
            }
            last_log_time = now;
        }

        log_window.poll();
        if (window.shouldClose(1)) {
            break;
        }
    }

    telemetry_thread.stop();
    return 0;
}

} // namespace gcs::app
