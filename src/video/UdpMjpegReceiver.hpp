#pragma once

#include "video/JpegFrameReassembler.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace gcs::video {

struct UdpMjpegReceiverStats {
    std::uint64_t packets_received = 0;
    std::uint64_t malformed_packets = 0;
    std::uint64_t completed_frames = 0;
    std::uint64_t incomplete_frames = 0;
    std::uint64_t old_packets = 0;
    std::uint64_t chunk_mismatch_resets = 0;
    std::uint64_t sender_restarts = 0;
    std::uint16_t last_chunk_count = 0;
    std::uint64_t last_frame_bytes = 0;
};

class UdpMjpegReceiver {
public:
    UdpMjpegReceiver();
    ~UdpMjpegReceiver();

    UdpMjpegReceiver(const UdpMjpegReceiver&) = delete;
    UdpMjpegReceiver& operator=(const UdpMjpegReceiver&) = delete;

    bool open(std::uint16_t port);
    std::optional<JpegFrame> receiveFrame(int timeout_ms);
    void close();
    std::string lastError() const;
    UdpMjpegReceiverStats stats() const;

private:
    std::uintptr_t socket_ = 0;
    bool socket_open_ = false;
    JpegFrameReassembler reassembler_;
    std::uint64_t packets_received_ = 0;
    std::uint64_t malformed_packets_ = 0;
    mutable std::string last_error_;
};

} // namespace gcs::video
