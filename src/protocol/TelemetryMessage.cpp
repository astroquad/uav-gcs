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

GridNodeTelemetry gridNodeOr(const nlohmann::json& object, GridNodeTelemetry fallback = {})
{
    if (!object.is_object()) {
        return fallback;
    }
    fallback.valid = valueOr<bool>(object, "valid", fallback.valid);
    fallback.id = valueOr<std::uint32_t>(object, "id", fallback.id);
    if (const auto coord = object.find("local_coord");
        coord != object.end() && coord->is_object()) {
        fallback.x = valueOr<int>(*coord, "x", fallback.x);
        fallback.y = valueOr<int>(*coord, "y", fallback.y);
    }
    fallback.topology = valueOr<std::string>(object, "topology", fallback.topology);
    fallback.arrival_heading =
        valueOr<std::string>(object, "arrival_heading", fallback.arrival_heading);
    fallback.camera_branch_mask =
        valueOr<int>(object, "camera_branch_mask", fallback.camera_branch_mask);
    fallback.grid_branch_mask =
        valueOr<int>(object, "grid_branch_mask", fallback.grid_branch_mask);
    fallback.first_node = valueOr<bool>(object, "first_node", fallback.first_node);
    fallback.origin_local_only =
        valueOr<bool>(object, "origin_local_only", fallback.origin_local_only);
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

    if (const auto system = json.find("system"); system != json.end()) {
        message.system.board_model =
            valueOr<std::string>(*system, "board_model", message.system.board_model);
        message.system.os_release =
            valueOr<std::string>(*system, "os_release", message.system.os_release);
        message.system.uptime_s =
            valueOr<double>(*system, "uptime_s", message.system.uptime_s);
        message.system.cpu_temp_c =
            valueOr<double>(*system, "cpu_temp_c", message.system.cpu_temp_c);
        message.system.throttled_raw =
            valueOr<std::string>(*system, "throttled_raw", message.system.throttled_raw);
        message.system.cpu_load_1m =
            valueOr<double>(*system, "cpu_load_1m", message.system.cpu_load_1m);
        message.system.mem_available_kb =
            valueOr<std::uint64_t>(*system, "mem_available_kb", message.system.mem_available_kb);
        message.system.wifi_signal_dbm =
            valueOr<double>(*system, "wifi_signal_dbm", message.system.wifi_signal_dbm);
        message.system.wifi_tx_bitrate_mbps =
            valueOr<double>(*system, "wifi_tx_bitrate_mbps", message.system.wifi_tx_bitrate_mbps);
    }

    if (const auto camera = json.find("camera"); camera != json.end()) {
        message.camera.status = valueOr<std::string>(*camera, "status", message.camera.status);
        message.camera.sensor_model =
            valueOr<std::string>(*camera, "sensor_model", message.camera.sensor_model);
        message.camera.camera_index = valueOr<int>(*camera, "camera_index", message.camera.camera_index);
        message.camera.width = valueOr<int>(*camera, "width", message.camera.width);
        message.camera.height = valueOr<int>(*camera, "height", message.camera.height);
        message.camera.fps = valueOr<double>(*camera, "fps", message.camera.fps);
        message.camera.configured_fps =
            valueOr<double>(*camera, "configured_fps", message.camera.configured_fps);
        message.camera.measured_capture_fps =
            valueOr<double>(*camera, "measured_capture_fps", message.camera.measured_capture_fps);
        message.camera.frame_seq = valueOr<std::uint32_t>(*camera, "frame_seq", message.camera.frame_seq);
        message.camera.autofocus_mode =
            valueOr<std::string>(*camera, "autofocus_mode", message.camera.autofocus_mode);
        message.camera.lens_position =
            valueOr<double>(*camera, "lens_position", message.camera.lens_position);
        message.camera.exposure_mode =
            valueOr<std::string>(*camera, "exposure_mode", message.camera.exposure_mode);
        message.camera.shutter_us = valueOr<int>(*camera, "shutter_us", message.camera.shutter_us);
        message.camera.gain = valueOr<double>(*camera, "gain", message.camera.gain);
        message.camera.awb = valueOr<std::string>(*camera, "awb", message.camera.awb);
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
        if (const auto intersection = vision->find("intersection");
            intersection != vision->end() && intersection->is_object()) {
            message.vision.intersection.valid =
                valueOr<bool>(*intersection, "valid", message.vision.intersection.valid);
            message.vision.intersection.detected =
                valueOr<bool>(*intersection, "detected", message.vision.intersection.detected);
            message.vision.intersection.type =
                valueOr<std::string>(*intersection, "type", message.vision.intersection.type);
            message.vision.intersection.raw_type =
                valueOr<std::string>(*intersection, "raw_type", message.vision.intersection.raw_type);
            message.vision.intersection.stable =
                valueOr<bool>(*intersection, "stable", message.vision.intersection.stable);
            message.vision.intersection.held =
                valueOr<bool>(*intersection, "held", message.vision.intersection.held);
            if (const auto center = intersection->find("center_px");
                center != intersection->end()) {
                message.vision.intersection.center_px = pointOr(*center);
            }
            if (const auto raw_center = intersection->find("raw_center_px");
                raw_center != intersection->end()) {
                message.vision.intersection.raw_center_px = pointOr(*raw_center);
            }
            message.vision.intersection.score =
                valueOr<double>(*intersection, "score", message.vision.intersection.score);
            message.vision.intersection.raw_score =
                valueOr<double>(*intersection, "raw_score", message.vision.intersection.raw_score);
            message.vision.intersection.branch_mask =
                valueOr<int>(*intersection, "branch_mask", message.vision.intersection.branch_mask);
            message.vision.intersection.branch_count =
                valueOr<int>(*intersection, "branch_count", message.vision.intersection.branch_count);
            message.vision.intersection.stable_frames =
                valueOr<int>(*intersection, "stable_frames", message.vision.intersection.stable_frames);
            message.vision.intersection.radius_px =
                valueOr<double>(*intersection, "radius_px", message.vision.intersection.radius_px);
            message.vision.intersection.selected_mask_index =
                valueOr<int>(*intersection, "selected_mask_index", message.vision.intersection.selected_mask_index);
            if (const auto branches = intersection->find("branches");
                branches != intersection->end() && branches->is_array()) {
                message.vision.intersection.branches.clear();
                for (const auto& branch_json : *branches) {
                    if (!branch_json.is_object()) {
                        continue;
                    }
                    BranchTelemetry branch;
                    branch.direction = valueOr<std::string>(branch_json, "direction", branch.direction);
                    branch.present = valueOr<bool>(branch_json, "present", branch.present);
                    branch.score = valueOr<double>(branch_json, "score", branch.score);
                    if (const auto endpoint = branch_json.find("endpoint_px");
                        endpoint != branch_json.end()) {
                        branch.endpoint_px = pointOr(*endpoint);
                    }
                    branch.angle_deg = valueOr<double>(branch_json, "angle_deg", branch.angle_deg);
                    message.vision.intersection.branches.push_back(branch);
                }
            }
            message.vision.intersection_detected = message.vision.intersection.detected;
            message.vision.intersection_score = message.vision.intersection.score;
        }

        if (const auto decision = vision->find("intersection_decision");
            decision != vision->end() && decision->is_object()) {
            message.vision.intersection_decision.state =
                valueOr<std::string>(*decision, "state", message.vision.intersection_decision.state);
            message.vision.intersection_decision.action =
                valueOr<std::string>(*decision, "action", message.vision.intersection_decision.action);
            message.vision.intersection_decision.accepted_type =
                valueOr<std::string>(*decision, "accepted_type", message.vision.intersection_decision.accepted_type);
            message.vision.intersection_decision.best_observed_type =
                valueOr<std::string>(*decision, "best_observed_type", message.vision.intersection_decision.best_observed_type);
            message.vision.intersection_decision.event_ready =
                valueOr<bool>(*decision, "event_ready", message.vision.intersection_decision.event_ready);
            message.vision.intersection_decision.turn_candidate =
                valueOr<bool>(*decision, "turn_candidate", message.vision.intersection_decision.turn_candidate);
            message.vision.intersection_decision.required_turn =
                valueOr<bool>(*decision, "required_turn", message.vision.intersection_decision.required_turn);
            message.vision.intersection_decision.front_available =
                valueOr<bool>(*decision, "front_available", message.vision.intersection_decision.front_available);
            message.vision.intersection_decision.node_recorded =
                valueOr<bool>(*decision, "node_recorded", message.vision.intersection_decision.node_recorded);
            message.vision.intersection_decision.cooldown_active =
                valueOr<bool>(*decision, "cooldown_active", message.vision.intersection_decision.cooldown_active);
            message.vision.intersection_decision.accepted_branch_mask =
                valueOr<int>(*decision, "accepted_branch_mask", message.vision.intersection_decision.accepted_branch_mask);
            message.vision.intersection_decision.window_frames =
                valueOr<int>(*decision, "window_frames", message.vision.intersection_decision.window_frames);
            message.vision.intersection_decision.age_ms =
                valueOr<int>(*decision, "age_ms", message.vision.intersection_decision.age_ms);
            message.vision.intersection_decision.confidence =
                valueOr<double>(*decision, "confidence", message.vision.intersection_decision.confidence);
            if (const auto center = decision->find("center_px");
                center != decision->end()) {
                message.vision.intersection_decision.center_px = pointOr(*center);
            }
            message.vision.intersection_decision.center_y_norm =
                valueOr<double>(*decision, "center_y_norm", message.vision.intersection_decision.center_y_norm);
            message.vision.intersection_decision.approach_phase =
                valueOr<std::string>(*decision, "approach_phase", message.vision.intersection_decision.approach_phase);
            message.vision.intersection_decision.overshoot_risk =
                valueOr<bool>(*decision, "overshoot_risk", message.vision.intersection_decision.overshoot_risk);
            message.vision.intersection_decision.too_late_to_turn =
                valueOr<bool>(*decision, "too_late_to_turn", message.vision.intersection_decision.too_late_to_turn);
            if (const auto branches = decision->find("branches");
                branches != decision->end() && branches->is_array()) {
                message.vision.intersection_decision.branches.clear();
                for (const auto& branch_json : *branches) {
                    if (!branch_json.is_object()) {
                        continue;
                    }
                    BranchEvidenceTelemetry branch;
                    branch.direction =
                        valueOr<std::string>(branch_json, "direction", branch.direction);
                    branch.present_frames =
                        valueOr<int>(branch_json, "present_frames", branch.present_frames);
                    branch.max_score =
                        valueOr<double>(branch_json, "max_score", branch.max_score);
                    branch.average_score =
                        valueOr<double>(branch_json, "average_score", branch.average_score);
                    message.vision.intersection_decision.branches.push_back(branch);
                }
            }
            if (const auto node = decision->find("node");
                node != decision->end()) {
                message.vision.intersection_decision.node = gridNodeOr(*node);
            }
        }

        if (const auto node = vision->find("grid_node");
            node != vision->end()) {
            message.vision.grid_node = gridNodeOr(*node);
        }

        // Cycle 13: drone fractional position from last committed grid node.
        if (const auto dp = vision->find("drone_position");
            dp != vision->end() && dp->is_object()) {
            message.vision.drone_position.valid =
                valueOr<bool>(*dp, "valid", message.vision.drone_position.valid);
            message.vision.drone_position.cell_progress =
                valueOr<double>(*dp, "cell_progress", message.vision.drone_position.cell_progress);
            message.vision.drone_position.grid_offset_x =
                valueOr<double>(*dp, "grid_offset_x", message.vision.drone_position.grid_offset_x);
            message.vision.drone_position.grid_offset_y =
                valueOr<double>(*dp, "grid_offset_y", message.vision.drone_position.grid_offset_y);
        }

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
        message.debug.intersection_latency_ms =
            valueOr<double>(*debug, "intersection_latency_ms", message.debug.intersection_latency_ms);
        message.debug.intersection_decision_latency_ms =
            valueOr<double>(*debug, "intersection_decision_latency_ms", message.debug.intersection_decision_latency_ms);
        message.debug.telemetry_build_ms =
            valueOr<double>(*debug, "telemetry_build_ms", message.debug.telemetry_build_ms);
        message.debug.telemetry_send_ms =
            valueOr<double>(*debug, "telemetry_send_ms", message.debug.telemetry_send_ms);
        message.debug.video_submit_ms =
            valueOr<double>(*debug, "video_submit_ms", message.debug.video_submit_ms);
        message.debug.video_send_ms =
            valueOr<double>(*debug, "video_send_ms", message.debug.video_send_ms);
        message.debug.capture_fps =
            valueOr<double>(*debug, "capture_fps", message.debug.capture_fps);
        message.debug.processing_fps =
            valueOr<double>(*debug, "processing_fps", message.debug.processing_fps);
        message.debug.debug_video_send_fps =
            valueOr<double>(*debug, "debug_video_send_fps", message.debug.debug_video_send_fps);
        message.debug.video_chunk_pacing_us =
            valueOr<int>(*debug, "video_chunk_pacing_us", message.debug.video_chunk_pacing_us);
        message.debug.cpu_temp_c =
            valueOr<double>(*debug, "cpu_temp_c", message.debug.cpu_temp_c);
        message.debug.telemetry_bytes =
            valueOr<std::uint64_t>(*debug, "telemetry_bytes", message.debug.telemetry_bytes);
        message.debug.video_jpeg_bytes =
            valueOr<std::uint64_t>(*debug, "video_jpeg_bytes", message.debug.video_jpeg_bytes);
        message.debug.video_sent_frames =
            valueOr<std::uint64_t>(*debug, "video_sent_frames", message.debug.video_sent_frames);
        message.debug.video_dropped_frames =
            valueOr<std::uint64_t>(*debug, "video_dropped_frames", message.debug.video_dropped_frames);
        message.debug.video_skipped_frames =
            valueOr<std::uint64_t>(*debug, "video_skipped_frames", message.debug.video_skipped_frames);
        message.debug.video_chunks_sent =
            valueOr<std::uint64_t>(*debug, "video_chunks_sent", message.debug.video_chunks_sent);
        message.debug.video_send_failures =
            valueOr<std::uint64_t>(*debug, "video_send_failures", message.debug.video_send_failures);
        message.debug.video_chunk_count =
            valueOr<int>(*debug, "video_chunk_count", message.debug.video_chunk_count);
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
