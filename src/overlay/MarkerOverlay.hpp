#pragma once

#include "overlay/OverlayPrimitive.hpp"
#include "protocol/TelemetryMessage.hpp"

#include <vector>

namespace gcs::overlay {

std::vector<OverlayPrimitive> buildMarkerOverlays(
    const std::vector<protocol::MarkerTelemetry>& markers);

} // namespace gcs::overlay
