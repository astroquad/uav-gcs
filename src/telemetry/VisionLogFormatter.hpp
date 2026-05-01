#pragma once

#include "protocol/TelemetryMessage.hpp"
#include "telemetry/TelemetryStore.hpp"

#include <string>

namespace gcs::telemetry {

std::string formatVisionLog(
    const VisionFrame& frame,
    const protocol::TelemetryStats& stats);

} // namespace gcs::telemetry
