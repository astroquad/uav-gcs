#pragma once

#include <cstdint>
#include <string>

namespace gcs::common {

struct NetworkConfig {
    std::string onboard_ip = "127.0.0.1";
    std::uint16_t telemetry_port = 14550;
    std::uint16_t command_port = 14551;
    std::uint16_t video_port = 5600;
    int telemetry_timeout_ms = 2000;
    int cmd_ack_timeout_ms = 1000;
    int cmd_retry_count = 3;
};

NetworkConfig loadNetworkConfig(const std::string& config_dir);

} // namespace gcs::common
