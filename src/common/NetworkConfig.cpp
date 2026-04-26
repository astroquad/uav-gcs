#include "common/NetworkConfig.hpp"

#include <toml++/toml.hpp>

namespace gcs::common {
namespace {

std::string joinConfigPath(const std::string& config_dir)
{
    if (config_dir.empty()) {
        return "config/network.toml";
    }
    const char last = config_dir.back();
    if (last == '/' || last == '\\') {
        return config_dir + "network.toml";
    }
    return config_dir + "/network.toml";
}

} // namespace

NetworkConfig loadNetworkConfig(const std::string& config_dir)
{
    NetworkConfig config;

    toml::table table;
    try {
        table = toml::parse_file(joinConfigPath(config_dir));
    } catch (const toml::parse_error&) {
        return config;
    }

    if (const auto onboard = table["onboard"]) {
        config.onboard_ip = onboard["ip"].value_or(config.onboard_ip);
        config.telemetry_port = static_cast<std::uint16_t>(
            onboard["telemetry_port"].value_or(static_cast<int>(config.telemetry_port)));
        config.command_port = static_cast<std::uint16_t>(
            onboard["command_port"].value_or(static_cast<int>(config.command_port)));
        config.video_port = static_cast<std::uint16_t>(
            onboard["video_port"].value_or(static_cast<int>(config.video_port)));
    }

    if (const auto connection = table["connection"]) {
        config.telemetry_timeout_ms =
            connection["telemetry_timeout_ms"].value_or(config.telemetry_timeout_ms);
        config.cmd_ack_timeout_ms =
            connection["cmd_ack_timeout_ms"].value_or(config.cmd_ack_timeout_ms);
        config.cmd_retry_count =
            connection["cmd_retry_count"].value_or(config.cmd_retry_count);
    }

    return config;
}

} // namespace gcs::common
