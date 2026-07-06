#include "network/TelemetryReassembler.hpp"

#include "video/VideoPacket.hpp"

namespace gcs::network {
namespace {

// A message id this far below the current one cannot be a stale chunk of
// the same stream; the sender restarted and its counter reset. Without
// this the reassembler rejects every chunked message from a restarted
// onboard process until the GCS itself restarts.
constexpr std::uint32_t kSenderRestartGap = 1000;

} // namespace

std::optional<std::string> TelemetryReassembler::acceptDatagram(
    const std::uint8_t* data,
    std::size_t size)
{
    if (data == nullptr || size == 0) {
        ++stats_.malformed_packets;
        return std::nullopt;
    }

    if (data[0] == '{') {
        ++stats_.legacy_messages;
        return std::string(reinterpret_cast<const char*>(data), size);
    }

    video::VideoPacketHeader header;
    if (!video::parseHeader(data, size, header, video::kTelemetryMagic) ||
        header.chunk_count == 0 || header.chunk_index >= header.chunk_count ||
        header.payload_size == 0 ||
        header.payload_size != size - video::kVideoHeaderSize) {
        ++stats_.malformed_packets;
        return std::nullopt;
    }

    const bool sender_restarted =
        current_message_id_ != 0 &&
        header.frame_id < current_message_id_ &&
        current_message_id_ - header.frame_id > kSenderRestartGap;
    if (sender_restarted) {
        ++stats_.sender_restarts;
    }

    if (current_message_id_ == 0 || header.frame_id > current_message_id_ ||
        sender_restarted) {
        noteIncompleteMessage();
        current_message_id_ = header.frame_id;
        expected_chunks_ = header.chunk_count;
        received_chunks_ = 0;
        chunks_.assign(expected_chunks_, {});
    } else if (header.frame_id < current_message_id_) {
        ++stats_.old_packets;
        return std::nullopt;
    }

    if (header.chunk_count != expected_chunks_) {
        ++stats_.chunk_mismatch_resets;
        noteIncompleteMessage();
        reset();
        return std::nullopt;
    }

    auto& chunk = chunks_[header.chunk_index];
    if (chunk.empty()) {
        ++received_chunks_;
    }
    chunk.assign(reinterpret_cast<const char*>(data) + video::kVideoHeaderSize,
                 header.payload_size);

    if (received_chunks_ != expected_chunks_) {
        return std::nullopt;
    }

    std::string message;
    for (const auto& part : chunks_) {
        message += part;
    }
    ++stats_.chunked_messages;
    reset();
    return message;
}

TelemetryReassemblerStats TelemetryReassembler::stats() const
{
    return stats_;
}

void TelemetryReassembler::noteIncompleteMessage()
{
    if (current_message_id_ != 0 && received_chunks_ > 0 &&
        received_chunks_ < expected_chunks_) {
        ++stats_.incomplete_messages;
    }
}

void TelemetryReassembler::reset()
{
    current_message_id_ = 0;
    expected_chunks_ = 0;
    received_chunks_ = 0;
    chunks_.clear();
}

} // namespace gcs::network
