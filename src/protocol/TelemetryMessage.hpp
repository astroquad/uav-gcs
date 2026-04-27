#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace gcs::protocol {

struct Point2f {
    double x = 0.0;
    double y = 0.0;
};

struct MarkerTelemetry {
    int id = -1;
    Point2f center_px;
    std::array<Point2f, 4> corners_px {};
    double orientation_deg = 0.0;
};

struct LineTelemetry {
    bool detected = false;
    Point2f tracking_point_px;
    Point2f centroid_px;
    double center_offset_px = 0.0;
    double angle_deg = 0.0;
    double confidence = 0.0;
    std::vector<Point2f> contour_px;
};

struct CameraTelemetry {
    std::string status;
    int width = 0;
    int height = 0;
    double fps = 0.0;
    std::uint32_t frame_seq = 0;
};

struct VisionTelemetry {
    bool line_detected = false;
    double line_offset = 0.0;
    double line_angle = 0.0;
    LineTelemetry line;
    bool intersection_detected = false;
    double intersection_score = 0.0;
    bool marker_detected = false;
    int marker_id = -1;
    int marker_count = 0;
    std::vector<MarkerTelemetry> markers;
};

struct GridTelemetry {
    int row = -1;
    int col = -1;
    double heading_deg = 0.0;
};

struct DebugTelemetry {
    double processing_latency_ms = 0.0;
    double aruco_latency_ms = 0.0;
    double line_latency_ms = 0.0;
};

struct TelemetryMessage {
    int protocol_version = 0;
    std::string type;
    std::uint32_t seq = 0;
    std::int64_t timestamp_ms = 0;
    std::string mission_state;
    CameraTelemetry camera;
    VisionTelemetry vision;
    GridTelemetry grid;
    DebugTelemetry debug;
    std::string raw_json;
};

struct TelemetryStats {
    bool has_latest_seq = false;
    std::uint32_t latest_seq = 0;
    std::uint64_t received_packets = 0;
    std::uint64_t dropped_packets = 0;
    std::uint64_t duplicate_packets = 0;
    std::uint64_t out_of_order_packets = 0;
    std::int64_t last_receive_timestamp_ms = 0;

    void observe(const TelemetryMessage& message);
};

std::optional<TelemetryMessage> parseTelemetryJson(const std::string& payload);

} // namespace gcs::protocol
