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
    // Cycle 23: three-panel layout — grid (top-left), markers (top-right),
    // detail (bottom). Existing two-arg update() keeps working (markers
    // panel just stays empty).
    bool update(const std::string& grid_text,
                const std::string& markers_text,
                const std::string& detail_text);
    void poll();
    bool available() const;

private:
    void* native_state_ = nullptr;
};

} // namespace gcs::ui
