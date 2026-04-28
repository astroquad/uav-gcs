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
    assert(frame->received_steady_ms > 0);
    assert(frame->data.size() == sizeof(a) + sizeof(b));
    auto stats = reassembler.stats();
    assert(stats.completed_frames == 1);
    assert(stats.incomplete_frames == 0);
    assert(stats.last_chunk_count == 2);

    assert(!reassembler.acceptPacket(header(2, 0, 3, sizeof(a)), a, sizeof(a)));
    assert(!reassembler.acceptPacket(header(3, 0, 1, sizeof(a)), a, sizeof(a)));
    stats = reassembler.stats();
    assert(stats.incomplete_frames == 1);

    return 0;
}
