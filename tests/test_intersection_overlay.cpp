#include "overlay/IntersectionOverlay.hpp"

#include <cassert>

int main()
{
    gcs::protocol::IntersectionTelemetry intersection;
    intersection.valid = true;
    intersection.detected = true;
    intersection.type = "T";
    intersection.raw_type = "T";
    intersection.stable = true;
    intersection.center_px = {320.0, 240.0};
    intersection.score = 0.86;
    intersection.branches.push_back({"front", true, 0.92, {320.0, 120.0}, -90.0});
    intersection.branches.push_back({"right", true, 0.81, {480.0, 240.0}, 0.0});
    intersection.branches.push_back({"back", false, 0.12, {320.0, 360.0}, 90.0});

    const auto overlays = gcs::overlay::buildIntersectionOverlays(intersection);
    assert(overlays.size() >= 8);

    bool saw_cyan_center = false;
    bool saw_yellow_branch = false;
    for (const auto& overlay : overlays) {
        if (overlay.type == gcs::overlay::OverlayPrimitive::Type::Circle &&
            overlay.circle.color.r == 0 &&
            overlay.circle.color.g == 220 &&
            overlay.circle.color.b == 255) {
            saw_cyan_center = true;
        }
        if (overlay.type == gcs::overlay::OverlayPrimitive::Type::Line &&
            overlay.line.color.r == 255 &&
            overlay.line.color.g == 220 &&
            overlay.line.color.b == 0) {
            saw_yellow_branch = true;
        }
    }

    assert(saw_cyan_center);
    assert(saw_yellow_branch);
    return 0;
}
