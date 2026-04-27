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
          "raw_detected": true,
          "filtered": true,
          "held": false,
          "rejected_jump": false,
          "tracking_point_px": { "x": 307.5, "y": 336.0 },
          "raw_tracking_point_px": { "x": 300.0, "y": 336.0 },
          "centroid_px": { "x": 310.0, "y": 260.0 },
          "center_offset_px": -12.5,
          "raw_center_offset_px": -20.0,
          "angle_deg": 3.0,
          "raw_angle_deg": 6.0,
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
        "read_frame_ms": 1.0,
        "jpeg_decode_ms": 0.7,
        "aruco_latency_ms": 1.0,
        "line_latency_ms": 2.0,
        "telemetry_build_ms": 0.2,
        "telemetry_send_ms": 0.1,
        "video_submit_ms": 0.1,
        "telemetry_bytes": 512,
        "video_jpeg_bytes": 12345,
        "video_sent_frames": 98,
        "video_dropped_frames": 1,
        "line_mask_count": 1,
        "line_contours_found": 3,
        "line_candidates_evaluated": 2,
        "line_roi_pixels": 45056,
        "line_selected_contour_points": 2
      }
    })json";

    const auto parsed = gcs::protocol::parseTelemetryJson(payload);
    assert(parsed);
    assert(parsed->camera.frame_seq == 99);
    assert(parsed->vision.line_detected);
    assert(parsed->vision.line.detected);
    assert(parsed->vision.line.raw_detected);
    assert(parsed->vision.line.filtered);
    assert(!parsed->vision.line.held);
    assert(parsed->vision.line.raw_center_offset_px == -20.0);
    assert(parsed->vision.line.contour_px.size() == 2);
    assert(parsed->debug.line_latency_ms == 2.0);
    assert(parsed->debug.line_contours_found == 3);
    assert(parsed->debug.video_jpeg_bytes == 12345);
    return 0;
}
