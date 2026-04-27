#pragma once

#include <cstdint>
#include <string>

namespace gcs::app {

struct VisionDebugOptions {
    std::uint16_t video_port = 5600;
    std::uint16_t telemetry_port = 14550;
    int video_timeout_ms = 2000;
    int telemetry_timeout_ms = 2000;
    int marker_log_interval_ms = 1500;
    std::string title = "Astroquad Vision Debug";
};

class VisionDebugApp {
public:
    int run(const VisionDebugOptions& options);
};

} // namespace gcs::app
