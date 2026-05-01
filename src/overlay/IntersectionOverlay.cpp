#include "overlay/IntersectionOverlay.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

namespace gcs::overlay {
namespace {

OverlayPrimitive makeLine(Point2f start, Point2f end, Color color, int thickness = 2)
{
    OverlayPrimitive primitive;
    primitive.type = OverlayPrimitive::Type::Line;
    primitive.line.start = start;
    primitive.line.end = end;
    primitive.line.color = color;
    primitive.line.thickness = thickness;
    return primitive;
}

OverlayPrimitive makeCircle(Point2f center, double radius, Color color, int thickness = -1)
{
    OverlayPrimitive primitive;
    primitive.type = OverlayPrimitive::Type::Circle;
    primitive.circle.center = center;
    primitive.circle.radius = radius;
    primitive.circle.color = color;
    primitive.circle.thickness = thickness;
    return primitive;
}

OverlayPrimitive makeText(Point2f origin, std::string text, Color color, double scale = 0.45)
{
    OverlayPrimitive primitive;
    primitive.type = OverlayPrimitive::Type::Text;
    primitive.text.origin = origin;
    primitive.text.text = std::move(text);
    primitive.text.color = color;
    primitive.text.scale = scale;
    primitive.text.thickness = 1;
    return primitive;
}

Point2f toOverlayPoint(const protocol::Point2f& point)
{
    return {point.x, point.y};
}

std::string shortDirection(const std::string& direction)
{
    if (direction == "front") {
        return "F";
    }
    if (direction == "right") {
        return "R";
    }
    if (direction == "back") {
        return "B";
    }
    if (direction == "left") {
        return "L";
    }
    return "?";
}

std::string typeLabel(const protocol::IntersectionTelemetry& intersection)
{
    std::ostringstream stream;
    stream << "IX " << intersection.type;
    if (intersection.held) {
        stream << " hold";
    } else if (!intersection.stable) {
        stream << " raw";
    }
    return stream.str();
}

std::string branchLabel(const protocol::BranchTelemetry& branch)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2)
           << shortDirection(branch.direction) << ' ' << branch.score;
    return stream.str();
}

std::string decisionLabel(const protocol::IntersectionDecisionTelemetry& decision)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2)
           << "DEC " << decision.accepted_type
           << " " << decision.state;
    if (decision.node.valid) {
        stream << " (" << decision.node.x << ',' << decision.node.y << ')';
    } else if (decision.turn_candidate) {
        stream << " turn?";
    }
    if (decision.overshoot_risk) {
        stream << " OVR";
    } else if (decision.approach_phase == "turn_zone") {
        stream << " ZONE";
    }
    return stream.str();
}

bool hasDecisionOverlay(const protocol::IntersectionDecisionTelemetry& decision)
{
    return decision.window_frames > 0 ||
           decision.event_ready ||
           decision.turn_candidate ||
           decision.node.valid;
}

void addArrowHead(
    std::vector<OverlayPrimitive>& overlays,
    Point2f center,
    Point2f endpoint,
    Color color)
{
    const double dx = endpoint.x - center.x;
    const double dy = endpoint.y - center.y;
    const double length = std::hypot(dx, dy);
    if (length < 1.0) {
        return;
    }
    const double ux = dx / length;
    const double uy = dy / length;
    const double px = -uy;
    const double py = ux;
    constexpr double arrow = 12.0;
    constexpr double spread = 5.0;
    const Point2f base {
        endpoint.x - ux * arrow,
        endpoint.y - uy * arrow,
    };
    overlays.push_back(makeLine(
        endpoint,
        {base.x + px * spread, base.y + py * spread},
        color,
        2));
    overlays.push_back(makeLine(
        endpoint,
        {base.x - px * spread, base.y - py * spread},
        color,
        2));
}

} // namespace

std::vector<OverlayPrimitive> buildIntersectionOverlays(
    const protocol::IntersectionTelemetry& intersection,
    int frame_width,
    int frame_height)
{
    return buildIntersectionOverlays(intersection, {}, frame_width, frame_height);
}

std::vector<OverlayPrimitive> buildIntersectionOverlays(
    const protocol::IntersectionTelemetry& intersection,
    const protocol::IntersectionDecisionTelemetry& decision,
    int frame_width,
    int frame_height)
{
    const Color cyan {0, 220, 255};
    const Color yellow {255, 220, 0};
    const Color white {255, 255, 255};

    std::vector<OverlayPrimitive> overlays;
    if (!intersection.valid) {
        return overlays;
    }

    const Point2f center = toOverlayPoint(intersection.center_px);
    (void)frame_width;
    if (frame_height > 0 && center.y > frame_height * 0.68) {
        return overlays;
    }

    overlays.reserve(4 + intersection.branches.size() * 2);
    for (const auto& branch : intersection.branches) {
        if (!branch.present) {
            continue;
        }
        const Point2f endpoint = toOverlayPoint(branch.endpoint_px);
        overlays.push_back(makeLine(center, endpoint, yellow, 2));
        addArrowHead(overlays, center, endpoint, yellow);
    }

    overlays.push_back(makeCircle(center, 6.0, cyan, -1));
    overlays.push_back(makeCircle(center, 8.0, white, 1));
    overlays.push_back(makeText(
        {center.x + 10.0, center.y - 10.0},
        typeLabel(intersection),
        cyan,
        0.42));
    (void)decision;
    return overlays;
}

} // namespace gcs::overlay
