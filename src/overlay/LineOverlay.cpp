#include "overlay/LineOverlay.hpp"

#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

namespace gcs::overlay {
namespace {

OverlayPrimitive makeLine(Point2f start, Point2f end, Color color, int thickness = 3)
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

OverlayPrimitive makeText(Point2f origin, std::string text, Color color)
{
    OverlayPrimitive primitive;
    primitive.type = OverlayPrimitive::Type::Text;
    primitive.text.origin = origin;
    primitive.text.text = std::move(text);
    primitive.text.color = color;
    primitive.text.scale = 0.45;
    primitive.text.thickness = 1;
    return primitive;
}

Point2f toOverlayPoint(const protocol::Point2f& point)
{
    return {point.x, point.y};
}

std::string lineLabel(const protocol::LineTelemetry& line)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1)
           << "LINE off " << line.center_offset_px
           << " angle " << line.angle_deg
           << " conf " << line.confidence;
    return stream.str();
}

} // namespace

std::vector<OverlayPrimitive> buildLineOverlays(
    const protocol::LineTelemetry& line)
{
    const Color magenta {255, 0, 255};
    const Color green {0, 255, 0};
    const Color white {255, 255, 255};

    std::vector<OverlayPrimitive> overlays;
    if (!line.detected) {
        return overlays;
    }

    if (line.contour_px.size() >= 2) {
        overlays.reserve(line.contour_px.size() + 2);
        for (std::size_t index = 0; index < line.contour_px.size(); ++index) {
            const auto start = toOverlayPoint(line.contour_px[index]);
            const auto end = toOverlayPoint(line.contour_px[(index + 1) % line.contour_px.size()]);
            overlays.push_back(makeLine(start, end, magenta, 3));
        }
    }

    const Point2f tracking_point = toOverlayPoint(line.tracking_point_px);
    overlays.push_back(makeCircle(tracking_point, 6.0, green, -1));
    overlays.push_back(makeCircle(tracking_point, 8.0, white, 1));
    overlays.push_back(makeText(
        {tracking_point.x + 8.0, tracking_point.y - 8.0},
        lineLabel(line),
        green));
    return overlays;
}

} // namespace gcs::overlay
