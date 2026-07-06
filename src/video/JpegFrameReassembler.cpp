#include "video/JpegFrameReassembler.hpp"

#include <algorithm>

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
    const bool is_parity = (header.flags & kVideoFlagFecParity) != 0;
    if (header.chunk_count == 0 || payload_size != header.payload_size) {
        return std::nullopt;
    }
    if (is_parity) {
        const int group_size = header.flags >> 8;
        if (payload_size != kVideoFecPayloadSize || group_size <= 0) {
            return std::nullopt;
        }
        const std::size_t group_count =
            (header.chunk_count + group_size - 1) / group_size;
        if (header.chunk_index >= group_count) {
            return std::nullopt;
        }
    } else if (header.chunk_index >= header.chunk_count) {
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
        startFrame(header);
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

    if (is_parity) {
        const int group_size = header.flags >> 8;
        if (fec_group_size_ == 0) {
            fec_group_size_ = group_size;
            const std::size_t group_count =
                (expected_chunks_ + group_size - 1) / group_size;
            parity_.assign(group_count, {});
        } else if (fec_group_size_ != group_size) {
            return std::nullopt;
        }
        frame_total_bytes_ =
            (static_cast<std::uint32_t>(payload[0]) << 24) |
            (static_cast<std::uint32_t>(payload[1]) << 16) |
            (static_cast<std::uint32_t>(payload[2]) << 8) |
            static_cast<std::uint32_t>(payload[3]);
        parity_[header.chunk_index].assign(
            payload + 4, payload + kVideoFecPayloadSize);
        tryFecRecover(header.chunk_index);
    } else {
        auto& chunk = chunks_[header.chunk_index];
        if (chunk.empty()) {
            ++received_chunks_;
        }
        chunk.assign(payload, payload + payload_size);
        if (fec_group_size_ > 0) {
            tryFecRecover(header.chunk_index /
                          static_cast<std::size_t>(fec_group_size_));
        }
    }

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

void JpegFrameReassembler::startFrame(const VideoPacketHeader& header)
{
    current_frame_id_ = header.frame_id;
    current_timestamp_ms_ = header.timestamp_ms;
    expected_chunks_ = header.chunk_count;
    received_chunks_ = 0;
    chunks_.assign(expected_chunks_, {});
    fec_group_size_ = 0;
    frame_total_bytes_ = 0;
    parity_.clear();
}

void JpegFrameReassembler::tryFecRecover(std::size_t group_index)
{
    if (fec_group_size_ <= 0 || group_index >= parity_.size() ||
        parity_[group_index].empty()) {
        return;
    }

    const std::size_t begin =
        group_index * static_cast<std::size_t>(fec_group_size_);
    const std::size_t end = std::min<std::size_t>(
        begin + static_cast<std::size_t>(fec_group_size_), expected_chunks_);
    std::size_t missing_index = 0;
    int missing_count = 0;
    for (std::size_t index = begin; index < end; ++index) {
        if (chunks_[index].empty()) {
            missing_index = index;
            ++missing_count;
        }
    }
    if (missing_count != 1) {
        return;
    }

    // Sender invariant: every data chunk except the last is exactly
    // kVideoMaxPayloadSize bytes, so the missing chunk's size is implied by
    // its index; the parity packet carries the frame's total byte count for
    // the final chunk.
    std::size_t missing_size = kVideoMaxPayloadSize;
    if (missing_index + 1 == expected_chunks_) {
        const std::size_t full_bytes =
            kVideoMaxPayloadSize * (static_cast<std::size_t>(expected_chunks_) - 1);
        if (frame_total_bytes_ <= full_bytes ||
            frame_total_bytes_ - full_bytes > kVideoMaxPayloadSize) {
            return;
        }
        missing_size = frame_total_bytes_ - full_bytes;
    }

    std::vector<std::uint8_t> recovered = parity_[group_index];
    for (std::size_t index = begin; index < end; ++index) {
        if (index == missing_index) {
            continue;
        }
        const auto& chunk = chunks_[index];
        for (std::size_t byte = 0; byte < chunk.size(); ++byte) {
            recovered[byte] ^= chunk[byte];
        }
    }

    chunks_[missing_index].assign(
        recovered.begin(),
        recovered.begin() + static_cast<std::ptrdiff_t>(missing_size));
    ++received_chunks_;
    ++stats_.fec_recovered_chunks;
}

void JpegFrameReassembler::reset()
{
    current_frame_id_ = 0;
    current_timestamp_ms_ = 0;
    expected_chunks_ = 0;
    received_chunks_ = 0;
    chunks_.clear();
    fec_group_size_ = 0;
    frame_total_bytes_ = 0;
    parity_.clear();
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
