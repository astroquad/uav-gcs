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
    bool raw_detected = false;
    bool filtered = false;
    bool held = false;
    bool rejected_jump = false;
    Point2f tracking_point_px;
    Point2f raw_tracking_point_px;
    Point2f centroid_px;
    double center_offset_px = 0.0;
    double raw_center_offset_px = 0.0;
    double angle_deg = 0.0;
    double raw_angle_deg = 0.0;
    double confidence = 0.0;
    std::vector<Point2f> contour_px;
};

struct CameraTelemetry {
    std::string status;
    std::string sensor_model;
    int camera_index = 0;
    int width = 0;
    int height = 0;
    double fps = 0.0;
    double configured_fps = 0.0;
    double measured_capture_fps = 0.0;
    std::uint32_t frame_seq = 0;
    std::string autofocus_mode;
    double lens_position = 0.0;
    std::string exposure_mode;
    int shutter_us = 0;
    double gain = 0.0;
    std::string awb;
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
    double read_frame_ms = 0.0;
    double jpeg_decode_ms = 0.0;
    double aruco_latency_ms = 0.0;
    double line_latency_ms = 0.0;
    double telemetry_build_ms = 0.0;
    double telemetry_send_ms = 0.0;
    double video_submit_ms = 0.0;
    double video_send_ms = 0.0;
    double capture_fps = 0.0;
    double processing_fps = 0.0;
    double debug_video_send_fps = 0.0;
    int video_chunk_pacing_us = 0;
    double cpu_temp_c = 0.0;
    std::uint64_t telemetry_bytes = 0;
    std::uint64_t video_jpeg_bytes = 0;
    std::uint64_t video_sent_frames = 0;
    std::uint64_t video_dropped_frames = 0;
    std::uint64_t video_skipped_frames = 0;
    std::uint64_t video_chunks_sent = 0;
    std::uint64_t video_send_failures = 0;
    int video_chunk_count = 0;
    int line_mask_count = 0;
    int line_contours_found = 0;
    int line_candidates_evaluated = 0;
    int line_roi_pixels = 0;
    int line_selected_contour_points = 0;
};

struct SystemTelemetry {
    std::string board_model;
    std::string os_release;
    double uptime_s = 0.0;
    double cpu_temp_c = 0.0;
    std::string throttled_raw;
    double cpu_load_1m = 0.0;
    std::uint64_t mem_available_kb = 0;
    double wifi_signal_dbm = 0.0;
    double wifi_tx_bitrate_mbps = 0.0;
};

struct TelemetryMessage {
    int protocol_version = 0;
    std::string type;
    std::uint32_t seq = 0;
    std::int64_t timestamp_ms = 0;
    std::string mission_state;
    SystemTelemetry system;
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
