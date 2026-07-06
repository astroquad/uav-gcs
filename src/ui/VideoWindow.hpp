#pragma once

#include "overlay/OverlayPrimitive.hpp"
#include "video/JpegFrameReassembler.hpp"

#include <string>
#include <vector>

namespace gcs::ui {

class VideoWindow {
public:
    explicit VideoWindow(std::string title);
    ~VideoWindow();

    VideoWindow(const VideoWindow&) = delete;
    VideoWindow& operator=(const VideoWindow&) = delete;

    bool showFrame(const video::JpegFrame& frame);
    bool showFrame(
        const video::JpegFrame& frame,
        const std::vector<overlay::OverlayPrimitive>& overlays);
    // overlay_space_width/height name the pixel space the overlay
    // coordinates were produced in (telemetry camera dims). 0 falls back to
    // the decoded frame dimensions — correct only when the sender did not
    // downscale the debug video.
    bool showFrame(
        const video::JpegFrame& frame,
        const std::vector<overlay::OverlayPrimitive>& overlays,
        int overlay_space_width,
        int overlay_space_height);
    void showStatus(const std::string& status);
    bool shouldClose(int wait_ms);

private:
    std::string title_;
    void* native_state_ = nullptr;
};

} // namespace gcs::ui
