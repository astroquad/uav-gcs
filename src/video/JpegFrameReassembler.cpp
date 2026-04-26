#include "video/JpegFrameReassembler.hpp"

namespace gcs::video {

std::optional<JpegFrame> JpegFrameReassembler::acceptPacket(
    const VideoPacketHeader& header,
    const std::uint8_t* payload,
    std::size_t payload_size)
{
    if (header.chunk_count == 0 || header.chunk_index >= header.chunk_count ||
        payload_size != header.payload_size) {
        return std::nullopt;
    }

    if (current_frame_id_ == 0 || header.frame_id > current_frame_id_) {
        current_frame_id_ = header.frame_id;
        current_timestamp_ms_ = header.timestamp_ms;
        expected_chunks_ = header.chunk_count;
        received_chunks_ = 0;
        chunks_.assign(expected_chunks_, {});
    } else if (header.frame_id < current_frame_id_) {
        return std::nullopt;
    }

    if (header.chunk_count != expected_chunks_) {
        reset();
        return std::nullopt;
    }

    auto& chunk = chunks_[header.chunk_index];
    if (chunk.empty()) {
        ++received_chunks_;
    }
    chunk.assign(payload, payload + payload_size);

    if (received_chunks_ != expected_chunks_) {
        return std::nullopt;
    }

    JpegFrame frame;
    frame.frame_id = current_frame_id_;
    frame.timestamp_ms = current_timestamp_ms_;
    for (const auto& part : chunks_) {
        frame.data.insert(frame.data.end(), part.begin(), part.end());
    }
    reset();
    return frame;
}

void JpegFrameReassembler::reset()
{
    current_frame_id_ = 0;
    current_timestamp_ms_ = 0;
    expected_chunks_ = 0;
    received_chunks_ = 0;
    chunks_.clear();
}

} // namespace gcs::video
