#include "protocol/TelemetryMessage.hpp"

#include <nlohmann/json.hpp>

#include <cstddef>

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

Point2f pointOr(const nlohmann::json& object, Point2f fallback = {})
{
    if (!object.is_object()) {
        return fallback;
    }
    fallback.x = valueOr<double>(object, "x", fallback.x);
    fallback.y = valueOr<double>(object, "y", fallback.y);
    return fallback;
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
        message.vision.line.detected = message.vision.line_detected;
        message.vision.line.center_offset_px = message.vision.line_offset;
        message.vision.line.angle_deg = message.vision.line_angle;

        if (const auto line = vision->find("line");
            line != vision->end() && line->is_object()) {
            message.vision.line.detected =
                valueOr<bool>(*line, "detected", message.vision.line.detected);
            message.vision.line.raw_detected =
                valueOr<bool>(*line, "raw_detected", message.vision.line.raw_detected);
            message.vision.line.filtered =
                valueOr<bool>(*line, "filtered", message.vision.line.filtered);
            message.vision.line.held =
                valueOr<bool>(*line, "held", message.vision.line.held);
            message.vision.line.rejected_jump =
                valueOr<bool>(*line, "rejected_jump", message.vision.line.rejected_jump);
            if (const auto tracking_point = line->find("tracking_point_px");
                tracking_point != line->end()) {
                message.vision.line.tracking_point_px = pointOr(*tracking_point);
            }
            if (const auto raw_tracking_point = line->find("raw_tracking_point_px");
                raw_tracking_point != line->end()) {
                message.vision.line.raw_tracking_point_px = pointOr(*raw_tracking_point);
            }
            if (const auto centroid = line->find("centroid_px"); centroid != line->end()) {
                message.vision.line.centroid_px = pointOr(*centroid);
            }
            message.vision.line.center_offset_px =
                valueOr<double>(*line, "center_offset_px", message.vision.line.center_offset_px);
            message.vision.line.raw_center_offset_px =
                valueOr<double>(*line, "raw_center_offset_px", message.vision.line.raw_center_offset_px);
            message.vision.line.angle_deg =
                valueOr<double>(*line, "angle_deg", message.vision.line.angle_deg);
            message.vision.line.raw_angle_deg =
                valueOr<double>(*line, "raw_angle_deg", message.vision.line.raw_angle_deg);
            message.vision.line.confidence =
                valueOr<double>(*line, "confidence", message.vision.line.confidence);
            if (const auto contour = line->find("contour_px");
                contour != line->end() && contour->is_array()) {
                message.vision.line.contour_px.clear();
                for (const auto& point_json : *contour) {
                    message.vision.line.contour_px.push_back(pointOr(point_json));
                }
            }
            message.vision.line_detected = message.vision.line.detected;
            message.vision.line_offset = message.vision.line.center_offset_px;
            message.vision.line_angle = message.vision.line.angle_deg;
        }

        message.vision.intersection_detected =
            valueOr<bool>(*vision, "intersection_detected", message.vision.intersection_detected);
        message.vision.intersection_score =
            valueOr<double>(*vision, "intersection_score", message.vision.intersection_score);
        message.vision.marker_detected = valueOr<bool>(*vision, "marker_detected", message.vision.marker_detected);
        message.vision.marker_id = valueOr<int>(*vision, "marker_id", message.vision.marker_id);
        message.vision.marker_count = valueOr<int>(*vision, "marker_count", message.vision.marker_count);

        if (const auto markers = vision->find("markers");
            markers != vision->end() && markers->is_array()) {
            message.vision.markers.clear();
            for (const auto& marker_json : *markers) {
                if (!marker_json.is_object()) {
                    continue;
                }

                MarkerTelemetry marker;
                marker.id = valueOr<int>(marker_json, "id", marker.id);
                if (const auto center = marker_json.find("center_px"); center != marker_json.end()) {
                    marker.center_px = pointOr(*center);
                }
                if (const auto corners = marker_json.find("corners_px");
                    corners != marker_json.end() && corners->is_array() && corners->size() >= marker.corners_px.size()) {
                    for (std::size_t index = 0; index < marker.corners_px.size(); ++index) {
                        marker.corners_px[index] = pointOr((*corners)[index]);
                    }
                }
                marker.orientation_deg = valueOr<double>(marker_json, "orientation_deg", marker.orientation_deg);
                if (marker.id >= 0) {
                    message.vision.markers.push_back(marker);
                }
            }
            message.vision.marker_count = static_cast<int>(message.vision.markers.size());
            message.vision.marker_detected = !message.vision.markers.empty();
            if (!message.vision.markers.empty()) {
                message.vision.marker_id = message.vision.markers.front().id;
            }
        }
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
        message.debug.read_frame_ms =
            valueOr<double>(*debug, "read_frame_ms", message.debug.read_frame_ms);
        message.debug.jpeg_decode_ms =
            valueOr<double>(*debug, "jpeg_decode_ms", message.debug.jpeg_decode_ms);
        message.debug.aruco_latency_ms =
            valueOr<double>(*debug, "aruco_latency_ms", message.debug.aruco_latency_ms);
        message.debug.line_latency_ms =
            valueOr<double>(*debug, "line_latency_ms", message.debug.line_latency_ms);
        message.debug.telemetry_build_ms =
            valueOr<double>(*debug, "telemetry_build_ms", message.debug.telemetry_build_ms);
        message.debug.telemetry_send_ms =
            valueOr<double>(*debug, "telemetry_send_ms", message.debug.telemetry_send_ms);
        message.debug.video_submit_ms =
            valueOr<double>(*debug, "video_submit_ms", message.debug.video_submit_ms);
        message.debug.telemetry_bytes =
            valueOr<std::uint64_t>(*debug, "telemetry_bytes", message.debug.telemetry_bytes);
        message.debug.video_jpeg_bytes =
            valueOr<std::uint64_t>(*debug, "video_jpeg_bytes", message.debug.video_jpeg_bytes);
        message.debug.video_sent_frames =
            valueOr<std::uint64_t>(*debug, "video_sent_frames", message.debug.video_sent_frames);
        message.debug.video_dropped_frames =
            valueOr<std::uint64_t>(*debug, "video_dropped_frames", message.debug.video_dropped_frames);
        message.debug.line_mask_count =
            valueOr<int>(*debug, "line_mask_count", message.debug.line_mask_count);
        message.debug.line_contours_found =
            valueOr<int>(*debug, "line_contours_found", message.debug.line_contours_found);
        message.debug.line_candidates_evaluated =
            valueOr<int>(*debug, "line_candidates_evaluated", message.debug.line_candidates_evaluated);
        message.debug.line_roi_pixels =
            valueOr<int>(*debug, "line_roi_pixels", message.debug.line_roi_pixels);
        message.debug.line_selected_contour_points =
            valueOr<int>(*debug, "line_selected_contour_points", message.debug.line_selected_contour_points);
    }

    return message;
}

} // namespace gcs::protocol
