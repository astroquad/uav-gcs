#pragma once

#include "protocol/TelemetryMessage.hpp"
#include "telemetry/TelemetryStore.hpp"

#include <string>

namespace gcs::telemetry {

std::string formatMarkerLog(
    const MarkerFrame& frame,
    const protocol::TelemetryStats& stats);

} // namespace gcs::telemetry
