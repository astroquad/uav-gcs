#include "protocol/TelemetryMessage.hpp"

#include <cassert>
#include <string>

int main()
{
    const std::string payload = R"json({
      "protocol_version": 1,
      "type": "TELEMETRY",
      "seq": 10,
      "timestamp_ms": 123456,
      "camera": {
        "status": "streaming",
        "width": 640,
        "height": 480,
        "fps": 15.0,
        "frame_seq": 99
      },
      "vision": {
        "line_detected": true,
        "line_offset": -12.5,
        "line_angle": 3.0,
        "line": {
          "detected": true,
          "tracking_point_px": { "x": 307.5, "y": 336.0 },
          "centroid_px": { "x": 310.0, "y": 260.0 },
          "center_offset_px": -12.5,
          "angle_deg": 3.0,
          "confidence": 0.8,
          "contour_px": [
            { "x": 280.0, "y": 120.0 },
            { "x": 350.0, "y": 120.0 }
          ]
        },
        "marker_detected": false,
        "marker_id": -1,
        "marker_count": 0,
        "markers": []
      },
      "debug": {
        "processing_latency_ms": 4.0,
        "aruco_latency_ms": 1.0,
        "line_latency_ms": 2.0
      }
    })json";

    const auto parsed = gcs::protocol::parseTelemetryJson(payload);
    assert(parsed);
    assert(parsed->camera.frame_seq == 99);
    assert(parsed->vision.line_detected);
    assert(parsed->vision.line.detected);
    assert(parsed->vision.line.contour_px.size() == 2);
    assert(parsed->debug.line_latency_ms == 2.0);
    return 0;
}
