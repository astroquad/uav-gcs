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
    frame.aruco_latency_ms = message.debug.aruco_latency_ms;
    frame.line_latency_ms = message.debug.line_latency_ms;
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
