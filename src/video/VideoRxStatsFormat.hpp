#pragma once

#include "video/UdpMjpegReceiver.hpp"

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace gcs::video {

// One-line receive diagnostics shared by astroquad-gcs and uav-gcs-video.
// packets=0 means datagrams never reach the socket (firewall/routing);
// packets>0 with completed=0 points at chunk loss or reassembly issues.
inline std::string formatVideoStatsLine(
    const UdpMjpegReceiverStats& stats,
    std::uint64_t overwritten_frames = 0,
    double display_fps = 0.0)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1)
           << "[video-rx] display_fps=" << display_fps
           << " packets=" << stats.packets_received
           << " completed=" << stats.completed_frames
           << " incomplete=" << stats.incomplete_frames
           << " malformed=" << stats.malformed_packets
           << " old_packets=" << stats.old_packets
           << " mismatch_resets=" << stats.chunk_mismatch_resets
           << " latest_overwritten=" << overwritten_frames
           << " last_chunks=" << stats.last_chunk_count
           << " last_bytes=" << stats.last_frame_bytes << "\n";
    return stream.str();
}

} // namespace gcs::video
