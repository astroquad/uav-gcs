#pragma once

#include "protocol/TelemetryMessage.hpp"
#include "telemetry/TelemetryStore.hpp"

#include <cstdint>
#include <string>

namespace gcs::telemetry {

std::string formatVisionLog(
    const VisionFrame& frame,
    const protocol::TelemetryStats& stats,
    std::int64_t now_ms);

} // namespace gcs::telemetry
