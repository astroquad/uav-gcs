#pragma once

#include <cstdint>
#include <string>

namespace gcs::app {

struct VideoViewerOptions {
    std::uint16_t port = 5600;
    int timeout_ms = 2000;
    std::string title = "Astroquad Camera";
};

class VideoViewerApp {
public:
    int run(const VideoViewerOptions& options);
};

} // namespace gcs::app
