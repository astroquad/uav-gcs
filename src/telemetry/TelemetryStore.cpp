#include "telemetry/TelemetryStore.hpp"

#include <utility>

namespace gcs::telemetry {
namespace {

std::int64_t absoluteDelta(std::int64_t left, std::int64_t right)
{
    return left > right ? left - right : right - left;
}

} // namespace

void TelemetryStore::observe(const protocol::TelemetryMessage& message)
{
    if (message.camera.frame_seq == 0) {
        return;
    }

    VisionFrame frame;
    frame.frame_seq = message.camera.frame_seq;
    frame.timestamp_ms = message.timestamp_ms;
    frame.width = message.camera.width;
    frame.height = message.camera.height;
    frame.processing_latency_ms = message.debug.processing_latency_ms;
    frame.read_frame_ms = message.debug.read_frame_ms;
    frame.jpeg_decode_ms = message.debug.jpeg_decode_ms;
    frame.aruco_latency_ms = message.debug.aruco_latency_ms;
    frame.line_latency_ms = message.debug.line_latency_ms;
    frame.telemetry_build_ms = message.debug.telemetry_build_ms;
    frame.telemetry_send_ms = message.debug.telemetry_send_ms;
    frame.video_submit_ms = message.debug.video_submit_ms;
    frame.video_send_ms = message.debug.video_send_ms;
    frame.cpu_temp_c = message.debug.cpu_temp_c;
    frame.telemetry_bytes = message.debug.telemetry_bytes;
    frame.video_jpeg_bytes = message.debug.video_jpeg_bytes;
    frame.video_sent_frames = message.debug.video_sent_frames;
    frame.video_dropped_frames = message.debug.video_dropped_frames;
    frame.video_skipped_frames = message.debug.video_skipped_frames;
    frame.video_chunks_sent = message.debug.video_chunks_sent;
    frame.video_chunk_count = message.debug.video_chunk_count;
    frame.line_mask_count = message.debug.line_mask_count;
    frame.line_contours_found = message.debug.line_contours_found;
    frame.line_candidates_evaluated = message.debug.line_candidates_evaluated;
    frame.line_roi_pixels = message.debug.line_roi_pixels;
    frame.line_selected_contour_points = message.debug.line_selected_contour_points;
    frame.markers = message.vision.markers;
    frame.line = message.vision.line;

    std::lock_guard<std::mutex> lock(mutex_);
    frames_.push_back(std::move(frame));
    while (frames_.size() > max_frames_) {
        frames_.pop_front();
    }
}

std::optional<VisionFrame> TelemetryStore::findForFrame(
    std::uint32_t frame_seq,
    std::uint64_t frame_timestamp_ms,
    int max_age_ms) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = frames_.rbegin(); it != frames_.rend(); ++it) {
        if (it->frame_seq == frame_seq) {
            return *it;
        }
    }

    if (frame_timestamp_ms == 0 || frames_.empty()) {
        return std::nullopt;
    }

    const auto target_timestamp = static_cast<std::int64_t>(frame_timestamp_ms);
    auto best_it = frames_.end();
    std::int64_t best_delta = static_cast<std::int64_t>(max_age_ms) + 1;
    for (auto it = frames_.begin(); it != frames_.end(); ++it) {
        const auto delta = absoluteDelta(it->timestamp_ms, target_timestamp);
        if (delta < best_delta) {
            best_delta = delta;
            best_it = it;
        }
    }

    if (best_it == frames_.end() || best_delta > max_age_ms) {
        return std::nullopt;
    }
    return *best_it;
}

std::optional<VisionFrame> TelemetryStore::latest() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (frames_.empty()) {
        return std::nullopt;
    }
    return frames_.back();
}

} // namespace gcs::telemetry
