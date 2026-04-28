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
      "system": {
        "board_model": "Raspberry Pi 4 Model B",
        "os_release": "Raspberry Pi OS",
        "uptime_s": 1234.0,
        "cpu_temp_c": 58.2,
        "throttled_raw": "throttled=0x0",
        "cpu_load_1m": 0.5,
        "mem_available_kb": 123456,
        "wifi_signal_dbm": -48.0,
        "wifi_tx_bitrate_mbps": 72.2
      },
      "camera": {
        "status": "streaming",
        "sensor_model": "imx519",
        "camera_index": 0,
        "width": 960,
        "height": 720,
        "fps": 12.0,
        "configured_fps": 12.0,
        "measured_capture_fps": 11.8,
        "frame_seq": 99,
        "autofocus_mode": "manual",
        "lens_position": 0.67,
        "exposure_mode": "sport",
        "shutter_us": 0,
        "gain": 0.0,
        "awb": "auto"
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
        "video_send_ms": 4.5,
        "capture_fps": 11.8,
        "processing_fps": 11.7,
        "debug_video_send_fps": 8.0,
        "video_chunk_pacing_us": 150,
        "cpu_temp_c": 62.5,
        "telemetry_bytes": 512,
        "video_jpeg_bytes": 12345,
        "video_sent_frames": 98,
        "video_dropped_frames": 1,
        "video_skipped_frames": 2,
        "video_chunks_sent": 120,
        "video_send_failures": 1,
        "video_chunk_count": 12,
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
    assert(parsed->system.board_model == "Raspberry Pi 4 Model B");
    assert(parsed->system.throttled_raw == "throttled=0x0");
    assert(parsed->system.mem_available_kb == 123456);
    assert(parsed->camera.sensor_model == "imx519");
    assert(parsed->camera.width == 960);
    assert(parsed->camera.height == 720);
    assert(parsed->camera.measured_capture_fps == 11.8);
    assert(parsed->camera.autofocus_mode == "manual");
    assert(parsed->camera.lens_position == 0.67);
    assert(parsed->vision.line_detected);
    assert(parsed->vision.line.detected);
    assert(parsed->vision.line.raw_detected);
    assert(parsed->vision.line.filtered);
    assert(!parsed->vision.line.held);
    assert(parsed->vision.line.raw_center_offset_px == -20.0);
    assert(parsed->vision.line.contour_px.size() == 2);
    assert(parsed->debug.line_latency_ms == 2.0);
    assert(parsed->debug.video_send_ms == 4.5);
    assert(parsed->debug.capture_fps == 11.8);
    assert(parsed->debug.processing_fps == 11.7);
    assert(parsed->debug.debug_video_send_fps == 8.0);
    assert(parsed->debug.video_chunk_pacing_us == 150);
    assert(parsed->debug.cpu_temp_c == 62.5);
    assert(parsed->debug.video_skipped_frames == 2);
    assert(parsed->debug.video_chunks_sent == 120);
    assert(parsed->debug.video_send_failures == 1);
    assert(parsed->debug.video_chunk_count == 12);
    assert(parsed->debug.line_contours_found == 3);
    assert(parsed->debug.video_jpeg_bytes == 12345);
    return 0;
}
