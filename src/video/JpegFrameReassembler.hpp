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

class JpegFrameReassembler {
public:
    std::optional<JpegFrame> acceptPacket(
        const VideoPacketHeader& header,
        const std::uint8_t* payload,
        std::size_t payload_size);

    void reset();

private:
    std::uint32_t current_frame_id_ = 0;
    std::uint64_t current_timestamp_ms_ = 0;
    std::uint16_t expected_chunks_ = 0;
    std::uint16_t received_chunks_ = 0;
    std::vector<std::vector<std::uint8_t>> chunks_;
};

} // namespace gcs::video
