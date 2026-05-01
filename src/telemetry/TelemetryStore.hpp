#pragma once

#include "protocol/TelemetryMessage.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <vector>

namespace gcs::telemetry {

struct VisionFrame {
    std::uint32_t frame_seq = 0;
    std::int64_t timestamp_ms = 0;
    protocol::SystemTelemetry system;
    protocol::CameraTelemetry camera;
    int width = 0;
    int height = 0;
    double processing_latency_ms = 0.0;
    double read_frame_ms = 0.0;
    double jpeg_decode_ms = 0.0;
    double aruco_latency_ms = 0.0;
    double line_latency_ms = 0.0;
    double intersection_latency_ms = 0.0;
    double intersection_decision_latency_ms = 0.0;
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
    std::vector<protocol::MarkerTelemetry> markers;
    protocol::LineTelemetry line;
    protocol::IntersectionTelemetry intersection;
    protocol::IntersectionDecisionTelemetry intersection_decision;
    protocol::GridNodeTelemetry grid_node;
};

using MarkerFrame = VisionFrame;

class TelemetryStore {
public:
    void observe(const protocol::TelemetryMessage& message);

    std::optional<VisionFrame> findForFrame(
        std::uint32_t frame_seq,
        std::uint64_t frame_timestamp_ms,
        int max_age_ms = 250) const;

    std::optional<VisionFrame> latest() const;

private:
    mutable std::mutex mutex_;
    std::deque<VisionFrame> frames_;
    std::size_t max_frames_ = 120;
};

} // namespace gcs::telemetry
