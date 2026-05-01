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
    bool update(const std::string& grid_text, const std::string& detail_text);
    void poll();
    bool available() const;

private:
    void* native_state_ = nullptr;
};

} // namespace gcs::ui
