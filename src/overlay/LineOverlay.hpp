#pragma once

#include "overlay/OverlayPrimitive.hpp"
#include "protocol/TelemetryMessage.hpp"

#include <vector>

namespace gcs::overlay {

std::vector<OverlayPrimitive> buildLineOverlays(
    const protocol::LineTelemetry& line,
    int frame_width = 0,
    int frame_height = 0);

} // namespace gcs::overlay
