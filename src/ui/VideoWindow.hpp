#pragma once

#include "video/JpegFrameReassembler.hpp"

#include <string>

namespace gcs::ui {

class VideoWindow {
public:
    explicit VideoWindow(std::string title);
    ~VideoWindow();

    VideoWindow(const VideoWindow&) = delete;
    VideoWindow& operator=(const VideoWindow&) = delete;

    bool showFrame(const video::JpegFrame& frame);
    void showStatus(const std::string& status);
    bool shouldClose(int wait_ms);

private:
    std::string title_;
    void* native_state_ = nullptr;
};

} // namespace gcs::ui
