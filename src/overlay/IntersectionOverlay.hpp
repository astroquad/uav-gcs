#pragma once

#include "overlay/OverlayPrimitive.hpp"
#include "protocol/TelemetryMessage.hpp"

#include <vector>

namespace gcs::overlay {

std::vector<OverlayPrimitive> buildIntersectionOverlays(
    const protocol::IntersectionTelemetry& intersection);

} // namespace gcs::overlay
