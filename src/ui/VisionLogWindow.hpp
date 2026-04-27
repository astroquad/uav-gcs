#pragma once

#include <string>

namespace gcs::ui {

class VisionLogWindow {
public:
    explicit VisionLogWindow(std::string title);
    ~VisionLogWindow();

    VisionLogWindow(const VisionLogWindow&) = delete;
    VisionLogWindow& operator=(const VisionLogWindow&) = delete;

    bool update(const std::string& text);
    void poll();
    bool available() const;

private:
    void* native_state_ = nullptr;
};

} // namespace gcs::ui
