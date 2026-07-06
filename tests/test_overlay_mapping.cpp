#include "overlay/OverlayMapping.hpp"

#include <cassert>
#include <cmath>

namespace {

bool nearlyEqual(double a, double b)
{
    return std::abs(a - b) < 1e-9;
}

} // namespace

int main()
{
    using gcs::overlay::mapLength;
    using gcs::overlay::mapToDisplay;
    using gcs::overlay::Point2f;
    using gcs::overlay::resolveOverlaySpace;

    // Identity when overlay space matches the destination image.
    {
        const auto mapped = mapToDisplay({480.0, 360.0}, 1456, 1088, 0, 0, 1456, 1088);
        assert(nearlyEqual(mapped.x, 480.0));
        assert(nearlyEqual(mapped.y, 360.0));
    }

    // Camera-space coordinates halve onto a 728x544 downscaled frame.
    {
        const auto mapped = mapToDisplay({480.0, 360.0}, 1456, 1088, 0, 0, 728, 544);
        assert(nearlyEqual(mapped.x, 240.0));
        assert(nearlyEqual(mapped.y, 180.0));
    }

    // Letterboxed destination rect applies its offset.
    {
        const auto mapped = mapToDisplay({728.0, 544.0}, 1456, 1088, 100, 50, 728, 544);
        assert(nearlyEqual(mapped.x, 100.0 + 364.0));
        assert(nearlyEqual(mapped.y, 50.0 + 272.0));
    }

    // Lengths (circle radii) scale by the axis ratio.
    assert(nearlyEqual(mapLength(50.0, 1456, 728), 25.0));
    assert(nearlyEqual(mapLength(50.0, 1456, 1456), 50.0));

    // resolveOverlaySpace: explicit space wins, 0 falls back to decoded dims.
    assert(resolveOverlaySpace(1456, 728) == 1456);
    assert(resolveOverlaySpace(0, 728) == 728);
    assert(resolveOverlaySpace(0, 0) == 1);

    return 0;
}
