// Tests are assert-based: keep assert() active even in Release
// builds (CMake adds -DNDEBUG there, which silently no-ops all checks).
#undef NDEBUG

#include "overlay/IntersectionOverlay.hpp"

#include <cassert>
#include <string>

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

    gcs::protocol::IntersectionDecisionTelemetry decision;
    decision.state = "node_record";
    decision.accepted_type = "T";
    decision.window_frames = 6;
    decision.node.valid = true;
    decision.node.x = 2;
    decision.node.y = 1;

    const auto overlays = gcs::overlay::buildIntersectionOverlays(intersection, decision, 640, 480);
    assert(overlays.size() >= 4);

    bool saw_cyan_center = false;
    bool saw_yellow_branch = false;
    bool saw_compact_label = false;
    bool saw_score_text = false;
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
        if (overlay.type == gcs::overlay::OverlayPrimitive::Type::Text &&
            overlay.text.text.find("IX T") != std::string::npos) {
            saw_compact_label = true;
        }
        if (overlay.type == gcs::overlay::OverlayPrimitive::Type::Text &&
            overlay.text.text.find("0.92") != std::string::npos) {
            saw_score_text = true;
        }
    }

    assert(saw_cyan_center);
    assert(saw_yellow_branch);
    assert(saw_compact_label);
    assert(!saw_score_text);
    return 0;
}
