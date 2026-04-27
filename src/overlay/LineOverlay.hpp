#pragma once

#include "overlay/OverlayPrimitive.hpp"
#include "protocol/TelemetryMessage.hpp"

#include <vector>

namespace gcs::overlay {

std::vector<OverlayPrimitive> buildLineOverlays(
    const protocol::LineTelemetry& line);

} // namespace gcs::overlay
