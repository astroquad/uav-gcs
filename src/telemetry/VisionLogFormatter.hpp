#pragma once

#include "protocol/TelemetryMessage.hpp"
#include "telemetry/TelemetryStore.hpp"

#include <string>

namespace gcs::telemetry {

std::string formatVisionLog(
    const VisionFrame& frame,
    const protocol::TelemetryStats& stats);

// Cycle 23: mission-aware overload. Prepends a "=== Mission ===" section
// with current state + control intent + snake completion flag. Existing
// 2-arg form keeps working for callers without mission context.
std::string formatVisionLog(
    const VisionFrame& frame,
    const protocol::MissionTelemetry& mission,
    const protocol::TelemetryStats& stats);

} // namespace gcs::telemetry
