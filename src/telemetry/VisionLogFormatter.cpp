#include "telemetry/VisionLogFormatter.hpp"

#include <cctype>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace gcs::telemetry {
namespace {

std::string branchSummary(const protocol::IntersectionTelemetry& intersection)
{
    std::ostringstream stream;
    for (std::size_t index = 0; index < intersection.branches.size(); ++index) {
        if (index > 0) {
            stream << ' ';
        }
        const auto& branch = intersection.branches[index];
        const char label = branch.direction.empty()
            ? '?'
            : static_cast<char>(std::toupper(static_cast<unsigned char>(branch.direction.front())));
        stream << label << ':' << branch.score;
    }
    return stream.str();
}

std::string decisionBranchSummary(const protocol::IntersectionDecisionTelemetry& decision)
{
    std::ostringstream stream;
    for (std::size_t index = 0; index < decision.branches.size(); ++index) {
        if (index > 0) {
            stream << ' ';
        }
        const auto& branch = decision.branches[index];
        const char label = branch.direction.empty()
            ? '?'
            : static_cast<char>(std::toupper(static_cast<unsigned char>(branch.direction.front())));
        stream << label << ':' << branch.present_frames << '/' << branch.max_score;
    }
    return stream.str();
}

bool markerRevisitEnabled(const protocol::MissionTelemetry& mission)
{
    return !mission.revisit_order.empty() && mission.revisit_order != "none";
}

bool markerRevisitSucceeded(const protocol::MissionTelemetry& mission)
{
    if (!markerRevisitEnabled(mission) || mission.markers_found.empty()) {
        return false;
    }
    if (mission.markers_expected > 0 &&
        static_cast<int>(mission.markers_found.size()) < mission.markers_expected) {
        return false;
    }
    return std::all_of(mission.markers_found.begin(), mission.markers_found.end(),
        [](const protocol::MissionMarkerEntry& marker) {
            return marker.revisited;
        });
}

} // namespace

namespace {
constexpr const char* kDivider = "------------------------------------------------------------";

// Format a millisecond mission timer as MM:SS.s (mm can exceed 59).
std::string formatMissionClock(std::int64_t elapsed_ms)
{
    if (elapsed_ms < 0) elapsed_ms = 0;
    const int total_tenths = static_cast<int>(elapsed_ms / 100);
    const int minutes = total_tenths / 600;
    const int seconds = (total_tenths / 10) % 60;
    const int tenths = total_tenths % 10;
    std::ostringstream clock;
    clock << std::setw(2) << std::setfill('0') << minutes << ':'
          << std::setw(2) << std::setfill('0') << seconds << '.' << tenths;
    return clock.str();
}
}

std::string formatVisionLog(
    const VisionFrame& frame,
    const protocol::MissionTelemetry& mission,
    const protocol::TelemetryStats& stats)
{
    std::ostringstream pre;
    if (mission.present) {
        pre << "=== Mission ===\n"
            << "elapsed=" << formatMissionClock(mission.mission_elapsed_ms) << "\n"
            << "state=" << (mission.state.empty() ? "(unknown)" : mission.state)
            << "  intent=" << (mission.control_intent.empty() ? "(unknown)" : mission.control_intent) << "\n"
            << "markers=" << mission.markers_found.size() << "/" << mission.markers_expected
            << "  snake_complete=" << (mission.snake_complete ? "yes" : "no");
        if (markerRevisitEnabled(mission)) {
            pre << "  marker_revisit="
                << (markerRevisitSucceeded(mission) ? "yes" : "no");
        }
        if (mission.return_active && !mission.return_phase.empty() &&
            mission.return_phase != "none") {
            pre << "  return=" << mission.return_phase;
        }
        pre << "\n";
        if (mission.mission_complete && mission.landing_success) {
            pre << "mission complete!\n";
        }
        pre << kDivider << "\n\n";
    }
    return pre.str() + formatVisionLog(frame, stats);
}

std::string formatVisionLog(
    const VisionFrame& frame,
    const protocol::TelemetryStats& stats)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1)
           << "=== Vision / Frame ===\n"
           << "[vision] frame=" << frame.frame_seq
           << " processing=" << frame.processing_latency_ms << "ms"
           << " read=" << frame.read_frame_ms << "ms"
           << " decode=" << frame.jpeg_decode_ms << "ms"
           << " aruco=" << frame.aruco_latency_ms << "ms"
           << " line=" << frame.line_latency_ms << "ms"
           << " ix=" << frame.intersection_latency_ms << "ms"
           << " dec=" << frame.intersection_decision_latency_ms << "ms"
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

    stream << "\n=== Vision / Line ===\n"
           << "[line] detected=" << (frame.line.detected ? "yes" : "no");
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

    stream << "\n=== Vision / Intersection ===\n"
           << "[intersection] type=" << frame.intersection.type
           << " raw=" << frame.intersection.raw_type
           << " stable=" << (frame.intersection.stable ? "yes" : "no")
           << " held=" << (frame.intersection.held ? "yes" : "no")
           << " valid=" << (frame.intersection.valid ? "yes" : "no")
           << " detected=" << (frame.intersection.detected ? "yes" : "no")
           << " score=" << frame.intersection.score
           << " raw_score=" << frame.intersection.raw_score
           << " center=(" << frame.intersection.center_px.x
           << ',' << frame.intersection.center_px.y << ")"
           << " raw_center=(" << frame.intersection.raw_center_px.x
           << ',' << frame.intersection.raw_center_px.y << ")"
           << " branches=" << branchSummary(frame.intersection)
           << " mask=" << frame.intersection.branch_mask
           << " count=" << frame.intersection.branch_count
           << " stable_frames=" << frame.intersection.stable_frames << "\n";

    stream << "[intersection-decision] state=" << frame.intersection_decision.state
           << " action=" << frame.intersection_decision.action
           << " accepted=" << frame.intersection_decision.accepted_type
           << " best=" << frame.intersection_decision.best_observed_type
           << " event=" << (frame.intersection_decision.event_ready ? "node" : "no")
           << " turn=" << (frame.intersection_decision.turn_candidate ? "yes" : "no")
           << " required_turn=" << (frame.intersection_decision.required_turn ? "yes" : "no")
           << " front=" << (frame.intersection_decision.front_available ? "yes" : "no")
           << " conf=" << frame.intersection_decision.confidence
           << " y=" << frame.intersection_decision.center_y_norm
           << " phase=" << frame.intersection_decision.approach_phase
           << " overshoot=" << (frame.intersection_decision.overshoot_risk ? "yes" : "no")
           << " window=" << frame.intersection_decision.window_frames
           << " mask=" << frame.intersection_decision.accepted_branch_mask
           << " branches=" << decisionBranchSummary(frame.intersection_decision) << "\n";

    stream << "\n=== Grid ===\n";
    if (frame.grid_node.valid) {
        stream << "[grid-node] id=" << frame.grid_node.id
               << " coord=(" << frame.grid_node.x << ',' << frame.grid_node.y << ")"
               << " topology=" << frame.grid_node.topology
               << " heading=" << frame.grid_node.arrival_heading
               << " camera_mask=" << frame.grid_node.camera_branch_mask
               << " grid_mask=" << frame.grid_node.grid_branch_mask
               << " first=" << (frame.grid_node.first_node ? "yes" : "no")
               << " origin=" << (frame.grid_node.origin_local_only ? "local" : "official")
               << "\n";
    } else {
        stream << "[grid-node] none\n";
    }

    stream << "\n=== Vision / Markers (current frame) ===\n"
           << "[markers] count=" << frame.markers.size() << "\n";
    if (frame.markers.empty()) {
        stream << "  (none)\n";
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
