#include "overlay/LineOverlay.hpp"

#include <cassert>

int main()
{
    gcs::protocol::LineTelemetry line;
    line.detected = true;
    line.tracking_point_px = {320.0, 336.0};
    line.center_offset_px = 0.0;
    line.angle_deg = 0.0;
    line.confidence = 0.75;
    line.contour_px.push_back({280.0, 120.0});
    line.contour_px.push_back({350.0, 120.0});
    line.contour_px.push_back({365.0, 470.0});
    line.contour_px.push_back({260.0, 470.0});

    const auto overlays = gcs::overlay::buildLineOverlays(line);
    assert(overlays.size() >= 6);
    assert(overlays.front().type == gcs::overlay::OverlayPrimitive::Type::Line);
    assert(overlays.front().line.color.r == 255);
    assert(overlays.front().line.color.g == 0);
    assert(overlays.front().line.color.b == 255);

    bool saw_green_circle = false;
    for (const auto& overlay : overlays) {
        if (overlay.type == gcs::overlay::OverlayPrimitive::Type::Circle &&
            overlay.circle.color.r == 0 &&
            overlay.circle.color.g == 255 &&
            overlay.circle.color.b == 0) {
            saw_green_circle = true;
        }
    }
    assert(saw_green_circle);
    return 0;
}
