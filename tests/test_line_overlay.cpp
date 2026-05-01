#include "overlay/LineOverlay.hpp"

#include <cassert>

int main()
{
    gcs::protocol::LineTelemetry line;
    line.detected = true;
    line.tracking_point_px = {320.0, 336.0};
    line.center_offset_px = 0.0;
    line.raw_detected = true;
    line.raw_tracking_point_px = {380.0, 336.0};
    line.raw_center_offset_px = 60.0;
    line.angle_deg = 0.0;
    line.confidence = 0.75;
    line.contour_px.push_back({280.0, 120.0});
    line.contour_px.push_back({350.0, 120.0});
    line.contour_px.push_back({365.0, 470.0});
    line.contour_px.push_back({260.0, 470.0});

    const auto overlays = gcs::overlay::buildLineOverlays(line, 640, 480);
    assert(overlays.size() >= 6);
    assert(overlays.front().type == gcs::overlay::OverlayPrimitive::Type::Line);
    assert(overlays.front().line.color.r == 255);
    assert(overlays.front().line.color.g == 0);
    assert(overlays.front().line.color.b == 255);

    bool saw_red_center_circle = false;
    bool saw_green_offset_line = false;
    for (const auto& overlay : overlays) {
        if (overlay.type == gcs::overlay::OverlayPrimitive::Type::Circle &&
            overlay.circle.color.r == 255 &&
            overlay.circle.color.g == 0 &&
            overlay.circle.color.b == 0) {
            saw_red_center_circle = true;
            assert(overlay.circle.center.x == 380.0);
            assert(overlay.circle.center.y == 240.0);
        }
        if (overlay.type == gcs::overlay::OverlayPrimitive::Type::Line &&
            overlay.line.color.r == 0 &&
            overlay.line.color.g == 255 &&
            overlay.line.color.b == 0 &&
            overlay.line.start.y == 240.0 &&
            overlay.line.end.y == 240.0) {
            saw_green_offset_line = true;
            assert(overlay.line.start.x == 320.0);
            assert(overlay.line.end.x == 380.0);
        }
    }
    assert(saw_red_center_circle);
    assert(saw_green_offset_line);
    return 0;
}
