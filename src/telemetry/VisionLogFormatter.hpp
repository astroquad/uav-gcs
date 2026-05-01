#pragma once

#include "protocol/TelemetryMessage.hpp"
#include "telemetry/TelemetryStore.hpp"

#include <string>

namespace gcs::telemetry {

std::string formatVisionLog(
    const VisionFrame& frame,
    const protocol::TelemetryStats& stats,
    const std::string& grid_map_text = {});

} // namespace gcs::telemetry
