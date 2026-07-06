#pragma once

#include "overlay/OverlayPrimitive.hpp"

#include <algorithm>
#include <cmath>

namespace gcs::overlay {

// Overlay primitives arrive in the onboard camera pixel space
// (telemetry camera.width/height), while the received debug-video JPEG may
// be downscaled by the sender. Returns the dimension overlays should be
// interpreted in: the explicit overlay space when provided (> 0), otherwise
// the decoded image dimension (the pre-downscale identity behavior).
inline int resolveOverlaySpace(int overlay_dim, int decoded_dim)
{
    return overlay_dim > 0 ? overlay_dim : std::max(1, decoded_dim);
}

// Maps a point from the overlay source space (src_w x src_h) onto the
// destination rect (dst_x, dst_y, dst_w, dst_h) the image is drawn into.
inline Point2f mapToDisplay(
    const Point2f& point,
    int src_w,
    int src_h,
    int dst_x,
    int dst_y,
    int dst_w,
    int dst_h)
{
    Point2f output;
    output.x = dst_x + point.x * dst_w / std::max(1, src_w);
    output.y = dst_y + point.y * dst_h / std::max(1, src_h);
    return output;
}

// Maps a length (e.g. a circle radius) along one axis between the spaces.
inline double mapLength(double length, int src_dim, int dst_dim)
{
    return length * dst_dim / std::max(1, src_dim);
}

} // namespace gcs::overlay
