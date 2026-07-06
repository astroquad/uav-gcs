#include "video/JpegFrameReassembler.hpp"

namespace gcs::video {
namespace {

// A frame id this far below the current one cannot be a stale/reordered
// chunk of the same stream; the sender restarted and its counter reset.
// Without this the reassembler rejects every packet from a restarted
// onboard process until the GCS itself restarts.
constexpr std::uint32_t kSenderRestartGap = 1000;

} // namespace

std::optional<JpegFrame> JpegFrameReassembler::acceptPacket(
    const VideoPacketHeader& header,
    const std::uint8_t* payload,
    std::size_t payload_size)
{
    if (header.chunk_count == 0 || header.chunk_index >= header.chunk_count ||
        payload_size != header.payload_size) {
        return std::nullopt;
    }

    const bool sender_restarted =
        current_frame_id_ != 0 &&
        header.frame_id < current_frame_id_ &&
        current_frame_id_ - header.frame_id > kSenderRestartGap;
    if (sender_restarted) {
        ++stats_.sender_restarts;
    }

    if (current_frame_id_ == 0 || header.frame_id > current_frame_id_ ||
        sender_restarted) {
        noteIncompleteFrame();
        current_frame_id_ = header.frame_id;
        current_timestamp_ms_ = header.timestamp_ms;
        expected_chunks_ = header.chunk_count;
        received_chunks_ = 0;
        chunks_.assign(expected_chunks_, {});
    } else if (header.frame_id < current_frame_id_) {
        ++stats_.old_packets;
        return std::nullopt;
    }

    if (header.chunk_count != expected_chunks_) {
        ++stats_.chunk_mismatch_resets;
        noteIncompleteFrame();
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
    ++stats_.completed_frames;
    stats_.last_chunk_count = expected_chunks_;
    stats_.last_frame_bytes = frame.data.size();
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

JpegReassemblerStats JpegFrameReassembler::stats() const
{
    return stats_;
}

void JpegFrameReassembler::noteIncompleteFrame()
{
    if (current_frame_id_ != 0 && received_chunks_ > 0 && received_chunks_ < expected_chunks_) {
        ++stats_.incomplete_frames;
    }
}

} // namespace gcs::video
