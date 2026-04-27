#include "overlay/MarkerOverlay.hpp"

#include <cmath>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>

namespace gcs::overlay {
namespace {

constexpr double kPi = 3.14159265358979323846;

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

std::string markerLabel(const protocol::MarkerTelemetry& marker)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1)
           << "ID " << marker.id
           << " C(" << marker.center_px.x << ',' << marker.center_px.y << ")"
           << " A " << marker.orientation_deg << "deg";
    return stream.str();
}

void appendDirectionArrow(
    std::vector<OverlayPrimitive>& overlays,
    const protocol::MarkerTelemetry& marker,
    Color color)
{
    const double angle = marker.orientation_deg * kPi / 180.0;
    const Point2f center = toOverlayPoint(marker.center_px);
    const double length = 42.0;
    const Point2f tip {
        center.x + std::cos(angle) * length,
        center.y + std::sin(angle) * length,
    };

    overlays.push_back(makeLine(center, tip, color, 2));

    constexpr double kHeadLength = 12.0;
    constexpr double kHeadAngle = 28.0 * kPi / 180.0;
    const Point2f left {
        tip.x - std::cos(angle - kHeadAngle) * kHeadLength,
        tip.y - std::sin(angle - kHeadAngle) * kHeadLength,
    };
    const Point2f right {
        tip.x - std::cos(angle + kHeadAngle) * kHeadLength,
        tip.y - std::sin(angle + kHeadAngle) * kHeadLength,
    };
    overlays.push_back(makeLine(tip, left, color, 2));
    overlays.push_back(makeLine(tip, right, color, 2));
}

} // namespace

std::vector<OverlayPrimitive> buildMarkerOverlays(
    const std::vector<protocol::MarkerTelemetry>& markers)
{
    const Color edge_green {0, 255, 0};
    const Color corner_blue {0, 80, 255};
    const Color center_red {255, 0, 0};
    const Color arrow_cyan {0, 220, 255};

    std::vector<OverlayPrimitive> overlays;
    overlays.reserve(markers.size() * 14);

    for (const auto& marker : markers) {
        for (std::size_t index = 0; index < marker.corners_px.size(); ++index) {
            const auto start = toOverlayPoint(marker.corners_px[index]);
            const auto end = toOverlayPoint(marker.corners_px[(index + 1) % marker.corners_px.size()]);
            overlays.push_back(makeLine(start, end, edge_green, 2));
            overlays.push_back(makeCircle(start, 3.0, corner_blue, -1));
        }

        const Point2f center = toOverlayPoint(marker.center_px);
        overlays.push_back(makeCircle(center, 4.0, center_red, -1));
        appendDirectionArrow(overlays, marker, arrow_cyan);
        overlays.push_back(makeText({center.x + 6.0, center.y - 8.0}, markerLabel(marker), center_red));
    }

    return overlays;
}

} // namespace gcs::overlay
