#include "app/VideoViewerApp.hpp"

#include "ui/VideoWindow.hpp"
#include "video/UdpMjpegReceiver.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>

namespace gcs::app {

int VideoViewerApp::run(const VideoViewerOptions& options)
{
    video::UdpMjpegReceiver receiver;
    if (!receiver.open(options.port)) {
        std::cerr << "failed to open UDP video receiver on port "
                  << options.port << ": " << receiver.lastError() << "\n";
        return 1;
    }

    ui::VideoWindow window(options.title);
    window.showStatus("waiting for video stream...");

    std::cout << "uav_gcs_video\n"
              << "  listen UDP port: " << options.port << "\n"
              << "  timeout_ms: " << options.timeout_ms << "\n"
              << "  press q or ESC in the video window to exit\n";

    auto last_frame_time = std::chrono::steady_clock::now();
    bool waiting_status_visible = true;
    while (true) {
        const int poll_timeout_ms = std::clamp(options.timeout_ms, 1, 50);
        const auto frame = receiver.receiveFrame(poll_timeout_ms);
        if (frame) {
            last_frame_time = std::chrono::steady_clock::now();
            waiting_status_visible = false;
            if (!window.showFrame(*frame)) {
                window.showStatus("failed to decode JPEG frame");
            }
        } else if (receiver.lastError() == "timeout") {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - last_frame_time);
            if (!waiting_status_visible && elapsed.count() >= options.timeout_ms) {
                window.showStatus("waiting for video stream...");
                waiting_status_visible = true;
            }
        } else {
            std::cerr << "video receive warning: " << receiver.lastError() << "\n";
        }

        if (window.shouldClose(1)) {
            break;
        }
    }

    return 0;
}

} // namespace gcs::app
