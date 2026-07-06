// Tests are assert-based: keep assert() active even in Release
// builds (CMake adds -DNDEBUG there, which silently no-ops all checks).
#undef NDEBUG

#include "video/JpegFrameReassembler.hpp"

#include <cassert>
#include <cstdint>

namespace {

gcs::video::VideoPacketHeader header(
    std::uint32_t frame_id,
    std::uint16_t chunk_index,
    std::uint16_t chunk_count,
    std::uint32_t payload_size)
{
    gcs::video::VideoPacketHeader output;
    output.frame_id = frame_id;
    output.timestamp_ms = 1000 + frame_id;
    output.chunk_index = chunk_index;
    output.chunk_count = chunk_count;
    output.payload_size = payload_size;
    return output;
}

} // namespace

int main()
{
    gcs::video::JpegFrameReassembler reassembler;

    const std::uint8_t a[] = {0xff, 0xd8};
    const std::uint8_t b[] = {0x01, 0x02, 0xff, 0xd9};

    assert(!reassembler.acceptPacket(header(1, 0, 2, sizeof(a)), a, sizeof(a)));
    const auto frame = reassembler.acceptPacket(header(1, 1, 2, sizeof(b)), b, sizeof(b));
    assert(frame);
    assert(frame->frame_id == 1);
    assert(frame->data.size() == sizeof(a) + sizeof(b));
    auto stats = reassembler.stats();
    assert(stats.completed_frames == 1);
    assert(stats.incomplete_frames == 0);
    assert(stats.last_chunk_count == 2);

    assert(!reassembler.acceptPacket(header(2, 0, 3, sizeof(a)), a, sizeof(a)));
    // A single-chunk frame completes immediately and abandons frame 2.
    const auto single = reassembler.acceptPacket(header(3, 0, 1, sizeof(a)), a, sizeof(a));
    assert(single);
    assert(single->frame_id == 3);
    stats = reassembler.stats();
    assert(stats.incomplete_frames == 1);
    assert(stats.completed_frames == 2);

    // Sender restart: a large frame_id regression while a partial frame is
    // pending starts a new stream instead of rejecting packets forever.
    assert(!reassembler.acceptPacket(header(500000, 0, 2, sizeof(a)), a, sizeof(a)));
    assert(!reassembler.acceptPacket(header(7, 0, 2, sizeof(a)), a, sizeof(a)));
    const auto restarted = reassembler.acceptPacket(header(7, 1, 2, sizeof(b)), b, sizeof(b));
    assert(restarted);
    assert(restarted->frame_id == 7);
    stats = reassembler.stats();
    assert(stats.sender_restarts == 1);
    assert(stats.incomplete_frames == 2);

    // A genuinely stale packet (small regression) is still rejected as old.
    assert(!reassembler.acceptPacket(header(200, 0, 2, sizeof(a)), a, sizeof(a)));
    assert(!reassembler.acceptPacket(header(199, 0, 2, sizeof(a)), a, sizeof(a)));
    stats = reassembler.stats();
    assert(stats.old_packets == 1);
    assert(stats.sender_restarts == 1);

    return 0;
}
