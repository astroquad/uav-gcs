#include "telemetry/MarkerLogFormatter.hpp"

#include <iomanip>
#include <sstream>

namespace gcs::telemetry {

std::string formatMarkerLog(
    const MarkerFrame& frame,
    const protocol::TelemetryStats& stats,
    std::int64_t now_ms)
{
    const auto age_ms = now_ms - frame.timestamp_ms;

    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1)
           << "[marker] frame=" << frame.frame_seq
           << " count=" << frame.markers.size()
           << " age=" << age_ms << "ms"
           << " aruco=" << frame.aruco_latency_ms << "ms"
           << " packets=" << stats.received_packets
           << " dropped=" << stats.dropped_packets
           << " dup=" << stats.duplicate_packets
           << " ooo=" << stats.out_of_order_packets << '\n';

    if (frame.markers.empty()) {
        stream << "  no markers\n";
        return stream.str();
    }

    for (const auto& marker : frame.markers) {
        stream << "  id=" << marker.id
               << " center=(" << marker.center_px.x << ',' << marker.center_px.y << ")"
               << " orientation=" << marker.orientation_deg << "deg\n";
    }

    return stream.str();
}

} // namespace gcs::telemetry
