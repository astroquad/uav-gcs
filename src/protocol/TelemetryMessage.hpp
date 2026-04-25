#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace gcs::protocol {

struct TelemetryMessage {
    int protocol_version = 0;
    std::string type;
    std::uint32_t seq = 0;
    std::int64_t timestamp_ms = 0;
    std::string mission_state;
    std::string camera_status;
    int frame_width = 0;
    int frame_height = 0;
    double measured_fps = 0.0;
    std::string raw_json;
};

std::optional<TelemetryMessage> parseTelemetryJson(const std::string& payload);

} // namespace gcs::protocol
