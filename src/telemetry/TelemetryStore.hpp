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
    int width = 0;
    int height = 0;
    double processing_latency_ms = 0.0;
    double aruco_latency_ms = 0.0;
    double line_latency_ms = 0.0;
    std::vector<protocol::MarkerTelemetry> markers;
    protocol::LineTelemetry line;
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
