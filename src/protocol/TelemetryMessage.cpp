#include "protocol/TelemetryMessage.hpp"

#include <nlohmann/json.hpp>

namespace gcs::protocol {
namespace {

template <typename T>
T valueOr(const nlohmann::json& object, const char* key, T fallback)
{
    if (!object.is_object()) {
        return fallback;
    }
    const auto it = object.find(key);
    if (it == object.end() || it->is_null()) {
        return fallback;
    }
    try {
        return it->get<T>();
    } catch (const nlohmann::json::exception&) {
        return fallback;
    }
}

} // namespace

void TelemetryStats::observe(const TelemetryMessage& message)
{
    ++received_packets;
    last_receive_timestamp_ms = message.timestamp_ms;

    if (!has_latest_seq) {
        has_latest_seq = true;
        latest_seq = message.seq;
        return;
    }

    if (message.seq == latest_seq) {
        ++duplicate_packets;
        return;
    }

    if (message.seq < latest_seq) {
        ++out_of_order_packets;
        return;
    }

    if (message.seq > latest_seq + 1) {
        dropped_packets += message.seq - latest_seq - 1;
    }
    latest_seq = message.seq;
}

std::optional<TelemetryMessage> parseTelemetryJson(const std::string& payload)
{
    nlohmann::json json;
    try {
        json = nlohmann::json::parse(payload);
    } catch (const nlohmann::json::exception&) {
        return std::nullopt;
    }

    if (!json.is_object()) {
        return std::nullopt;
    }

    if (!json.contains("protocol_version") || !json.contains("type") ||
        !json.contains("seq") || !json.contains("timestamp_ms")) {
        return std::nullopt;
    }

    TelemetryMessage message;
    message.raw_json = payload;

    try {
        message.protocol_version = json.at("protocol_version").get<int>();
        message.type = json.at("type").get<std::string>();
        message.seq = json.at("seq").get<std::uint32_t>();
        message.timestamp_ms = json.at("timestamp_ms").get<std::int64_t>();
    } catch (const nlohmann::json::exception&) {
        return std::nullopt;
    }

    if (const auto mission = json.find("mission"); mission != json.end()) {
        message.mission_state = valueOr<std::string>(*mission, "state", message.mission_state);
    }

    if (const auto camera = json.find("camera"); camera != json.end()) {
        message.camera.status = valueOr<std::string>(*camera, "status", message.camera.status);
        message.camera.width = valueOr<int>(*camera, "width", message.camera.width);
        message.camera.height = valueOr<int>(*camera, "height", message.camera.height);
        message.camera.fps = valueOr<double>(*camera, "fps", message.camera.fps);
        message.camera.frame_seq = valueOr<std::uint32_t>(*camera, "frame_seq", message.camera.frame_seq);
    }

    // Backward-compatible parsing for earlier bring-up packets.
    if (const auto bringup = json.find("bringup"); bringup != json.end()) {
        message.camera.status = valueOr<std::string>(*bringup, "camera_status", message.camera.status);
        message.camera.width = valueOr<int>(*bringup, "frame_width", message.camera.width);
        message.camera.height = valueOr<int>(*bringup, "frame_height", message.camera.height);
        message.camera.fps = valueOr<double>(*bringup, "measured_fps", message.camera.fps);
    }

    if (const auto vision = json.find("vision"); vision != json.end()) {
        message.vision.line_detected = valueOr<bool>(*vision, "line_detected", message.vision.line_detected);
        message.vision.line_offset = valueOr<double>(*vision, "line_offset", message.vision.line_offset);
        message.vision.line_angle = valueOr<double>(*vision, "line_angle", message.vision.line_angle);
        message.vision.intersection_detected =
            valueOr<bool>(*vision, "intersection_detected", message.vision.intersection_detected);
        message.vision.intersection_score =
            valueOr<double>(*vision, "intersection_score", message.vision.intersection_score);
        message.vision.marker_detected = valueOr<bool>(*vision, "marker_detected", message.vision.marker_detected);
        message.vision.marker_id = valueOr<int>(*vision, "marker_id", message.vision.marker_id);
    }

    if (const auto grid = json.find("grid"); grid != json.end()) {
        message.grid.row = valueOr<int>(*grid, "row", message.grid.row);
        message.grid.col = valueOr<int>(*grid, "col", message.grid.col);
        message.grid.heading_deg = valueOr<double>(*grid, "heading_deg", message.grid.heading_deg);
    }

    if (const auto grid_pose = json.find("grid_pose"); grid_pose != json.end()) {
        message.grid.row = valueOr<int>(*grid_pose, "row", message.grid.row);
        message.grid.col = valueOr<int>(*grid_pose, "col", message.grid.col);
        message.grid.heading_deg = valueOr<double>(*grid_pose, "heading_deg", message.grid.heading_deg);
    }

    if (const auto debug = json.find("debug"); debug != json.end()) {
        message.debug.processing_latency_ms =
            valueOr<double>(*debug, "processing_latency_ms", message.debug.processing_latency_ms);
    }

    return message;
}

} // namespace gcs::protocol
