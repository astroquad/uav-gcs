#include "telemetry/VisionLogFormatter.hpp"

#include <iomanip>
#include <sstream>

namespace gcs::telemetry {

std::string formatVisionLog(
    const VisionFrame& frame,
    const protocol::TelemetryStats& stats,
    std::int64_t now_ms)
{
    const auto age_ms = now_ms - frame.timestamp_ms;

    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1)
           << "[vision] frame=" << frame.frame_seq
           << " age=" << age_ms << "ms"
           << " processing=" << frame.processing_latency_ms << "ms"
           << " read=" << frame.read_frame_ms << "ms"
           << " decode=" << frame.jpeg_decode_ms << "ms"
           << " aruco=" << frame.aruco_latency_ms << "ms"
           << " line=" << frame.line_latency_ms << "ms"
           << " json=" << frame.telemetry_build_ms << "ms"
           << " tsend=" << frame.telemetry_send_ms << "ms"
           << " vsubmit=" << frame.video_submit_ms << "ms"
           << " vsend=" << frame.video_send_ms << "ms"
           << " capture_fps=" << frame.capture_fps
           << " processing_fps=" << frame.processing_fps
           << " cpu_temp=" << frame.cpu_temp_c << "C\n"
           << "[camera] sensor=" << (frame.camera.sensor_model.empty() ? "unknown" : frame.camera.sensor_model)
           << " index=" << frame.camera.camera_index
           << " size=" << frame.width << "x" << frame.height
           << " configured_fps=" << frame.camera.configured_fps
           << " measured_capture_fps=" << frame.camera.measured_capture_fps
           << " af=" << (frame.camera.autofocus_mode.empty() ? "unknown" : frame.camera.autofocus_mode)
           << " lens=" << frame.camera.lens_position
           << " exposure=" << (frame.camera.exposure_mode.empty() ? "unknown" : frame.camera.exposure_mode)
           << " shutter_us=" << frame.camera.shutter_us
           << " gain=" << frame.camera.gain
           << " awb=" << (frame.camera.awb.empty() ? "unknown" : frame.camera.awb) << "\n"
           << "[system] board=" << (frame.system.board_model.empty() ? "unknown" : frame.system.board_model)
           << " os=" << (frame.system.os_release.empty() ? "unknown" : frame.system.os_release)
           << " uptime_s=" << frame.system.uptime_s
           << " load1=" << frame.system.cpu_load_1m
           << " mem_avail_kb=" << frame.system.mem_available_kb
           << " throttled=" << (frame.system.throttled_raw.empty() ? "unknown" : frame.system.throttled_raw)
           << " wifi_signal=" << frame.system.wifi_signal_dbm << "dBm"
           << " wifi_tx=" << frame.system.wifi_tx_bitrate_mbps << "Mbps\n"
           << "[packets] received=" << stats.received_packets
           << " dropped=" << stats.dropped_packets
           << " dup=" << stats.duplicate_packets
           << " ooo=" << stats.out_of_order_packets
           << " telemetry_bytes=" << frame.telemetry_bytes
           << " jpeg_bytes=" << frame.video_jpeg_bytes
           << " chunks_last=" << frame.video_chunk_count
           << " chunks_total=" << frame.video_chunks_sent
           << " chunk_pacing_us=" << frame.video_chunk_pacing_us
           << " video_target_fps=" << frame.debug_video_send_fps
           << " video_sent=" << frame.video_sent_frames
           << " video_dropped=" << frame.video_dropped_frames
           << " video_skipped=" << frame.video_skipped_frames
           << " video_send_failures=" << frame.video_send_failures << "\n";

    stream << "[line] detected=" << (frame.line.detected ? "yes" : "no");
    if (frame.line.detected) {
        stream << " tracking=(" << frame.line.tracking_point_px.x
               << ',' << frame.line.tracking_point_px.y << ")"
               << " offset=" << frame.line.center_offset_px
               << " angle=" << frame.line.angle_deg
               << " confidence=" << frame.line.confidence
               << " contour_points=" << frame.line.contour_px.size();
    }
    stream << "\n";

    stream << "[line-debug] raw=" << (frame.line.raw_detected ? "yes" : "no")
           << " filtered=" << (frame.line.filtered ? "yes" : "no")
           << " held=" << (frame.line.held ? "yes" : "no")
           << " rejected_jump=" << (frame.line.rejected_jump ? "yes" : "no")
           << " raw_tracking=(" << frame.line.raw_tracking_point_px.x
           << ',' << frame.line.raw_tracking_point_px.y << ")"
           << " raw_offset=" << frame.line.raw_center_offset_px
           << " raw_angle=" << frame.line.raw_angle_deg
           << " masks=" << frame.line_mask_count
           << " contours=" << frame.line_contours_found
           << " candidates=" << frame.line_candidates_evaluated
           << " roi_px=" << frame.line_roi_pixels
           << " selected_points=" << frame.line_selected_contour_points << "\n";

    stream << "[markers] count=" << frame.markers.size() << "\n";
    if (frame.markers.empty()) {
        stream << "  no markers\n";
    } else {
        for (const auto& marker : frame.markers) {
            stream << "  id=" << marker.id
                   << " center=(" << marker.center_px.x << ',' << marker.center_px.y << ")"
                   << " orientation=" << marker.orientation_deg << "deg\n";
        }
    }

    return stream.str();
}

} // namespace gcs::telemetry
