#pragma once

#include "video/VideoPacket.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace gcs::video {

struct JpegFrame {
    std::uint32_t frame_id = 0;
    std::uint64_t timestamp_ms = 0;
    std::vector<std::uint8_t> data;
};

struct JpegReassemblerStats {
    std::uint64_t completed_frames = 0;
    std::uint64_t incomplete_frames = 0;
    std::uint64_t old_packets = 0;
    std::uint64_t chunk_mismatch_resets = 0;
    std::uint64_t sender_restarts = 0;
    std::uint64_t fec_recovered_chunks = 0;
    std::uint16_t last_chunk_count = 0;
    std::uint64_t last_frame_bytes = 0;
};

class JpegFrameReassembler {
public:
    std::optional<JpegFrame> acceptPacket(
        const VideoPacketHeader& header,
        const std::uint8_t* payload,
        std::size_t payload_size);

    void reset();
    JpegReassemblerStats stats() const;

private:
    void noteIncompleteFrame();
    void startFrame(const VideoPacketHeader& header);
    // Reconstructs the single missing data chunk of an FEC group when its
    // parity packet is available.
    void tryFecRecover(std::size_t group_index);

    std::uint32_t current_frame_id_ = 0;
    std::uint64_t current_timestamp_ms_ = 0;
    std::uint16_t expected_chunks_ = 0;
    std::uint16_t received_chunks_ = 0;
    std::vector<std::vector<std::uint8_t>> chunks_;
    // FEC state for the current frame; group size arrives in parity flags.
    int fec_group_size_ = 0;
    std::uint32_t frame_total_bytes_ = 0;
    std::vector<std::vector<std::uint8_t>> parity_;
    JpegReassemblerStats stats_;
};

} // namespace gcs::video
