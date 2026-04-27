#pragma once

#include <cstdint>
#include <string>

namespace gcs::overlay {

struct Color {
    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;
};

struct Point2f {
    double x = 0.0;
    double y = 0.0;
};

struct OverlayLine {
    Point2f start;
    Point2f end;
    Color color;
    int thickness = 2;
};

struct OverlayCircle {
    Point2f center;
    double radius = 3.0;
    Color color;
    int thickness = -1;
};

struct OverlayText {
    Point2f origin;
    std::string text;
    Color color;
    double scale = 0.5;
    int thickness = 1;
};

struct OverlayPrimitive {
    enum class Type {
        Line,
        Circle,
        Text,
    };

    Type type = Type::Line;
    OverlayLine line;
    OverlayCircle circle;
    OverlayText text;
};

} // namespace gcs::overlay
