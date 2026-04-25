#include "protocol/TelemetryMessage.hpp"

#include <regex>

namespace gcs::protocol {
namespace {

std::optional<std::string> extractString(const std::string& payload, const std::string& key)
{
    const std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch match;
    if (std::regex_search(payload, match, pattern) && match.size() >= 2) {
        return match[1].str();
    }
    return std::nullopt;
}

std::optional<long long> extractInteger(const std::string& payload, const std::string& key)
{
    const std::regex pattern("\"" + key + "\"\\s*:\\s*(-?[0-9]+)");
    std::smatch match;
    if (std::regex_search(payload, match, pattern) && match.size() >= 2) {
        try {
            return std::stoll(match[1].str());
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

std::optional<double> extractDouble(const std::string& payload, const std::string& key)
{
    const std::regex pattern("\"" + key + "\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?)");
    std::smatch match;
    if (std::regex_search(payload, match, pattern) && match.size() >= 2) {
        try {
            return std::stod(match[1].str());
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

} // namespace

std::optional<TelemetryMessage> parseTelemetryJson(const std::string& payload)
{
    TelemetryMessage message;
    message.raw_json = payload;

    const auto protocol_version = extractInteger(payload, "protocol_version");
    const auto type = extractString(payload, "type");
    const auto seq = extractInteger(payload, "seq");
    const auto timestamp_ms = extractInteger(payload, "timestamp_ms");

    if (!protocol_version || !type || !seq || !timestamp_ms) {
        return std::nullopt;
    }

    message.protocol_version = static_cast<int>(*protocol_version);
    message.type = *type;
    message.seq = static_cast<std::uint32_t>(*seq);
    message.timestamp_ms = static_cast<std::int64_t>(*timestamp_ms);

    if (const auto state = extractString(payload, "state")) {
        message.mission_state = *state;
    }
    if (const auto camera_status = extractString(payload, "camera_status")) {
        message.camera_status = *camera_status;
    }
    if (const auto frame_width = extractInteger(payload, "frame_width")) {
        message.frame_width = static_cast<int>(*frame_width);
    }
    if (const auto frame_height = extractInteger(payload, "frame_height")) {
        message.frame_height = static_cast<int>(*frame_height);
    }
    if (const auto measured_fps = extractDouble(payload, "measured_fps")) {
        message.measured_fps = *measured_fps;
    }

    return message;
}

} // namespace gcs::protocol
