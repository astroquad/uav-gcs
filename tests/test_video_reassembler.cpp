// Tests are assert-based: keep assert() active even in Release
// builds (CMake adds -DNDEBUG there, which silently no-ops all checks).
#undef NDEBUG

#include "video/JpegFrameReassembler.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

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

// Splits `frame` into wire-layout data chunks (all full-size except the
// last) plus one XOR parity payload per group, mirroring UdpMjpegStreamer.
struct FecFrame {
    std::vector<std::vector<std::uint8_t>> chunks;
    std::vector<std::vector<std::uint8_t>> parity;
};

FecFrame buildFecFrame(const std::vector<std::uint8_t>& frame, int group_size)
{
    using gcs::video::kVideoMaxPayloadSize;
    FecFrame output;
    const std::size_t chunk_count =
        (frame.size() + kVideoMaxPayloadSize - 1) / kVideoMaxPayloadSize;
    for (std::size_t index = 0; index < chunk_count; ++index) {
        const std::size_t offset = index * kVideoMaxPayloadSize;
        const std::size_t size =
            std::min(kVideoMaxPayloadSize, frame.size() - offset);
        output.chunks.emplace_back(
            frame.begin() + static_cast<std::ptrdiff_t>(offset),
            frame.begin() + static_cast<std::ptrdiff_t>(offset + size));
    }
    const std::size_t group_count =
        (chunk_count + group_size - 1) / group_size;
    for (std::size_t group = 0; group < group_count; ++group) {
        std::vector<std::uint8_t> parity(gcs::video::kVideoFecPayloadSize, 0);
        const auto total = static_cast<std::uint32_t>(frame.size());
        parity[0] = static_cast<std::uint8_t>((total >> 24) & 0xff);
        parity[1] = static_cast<std::uint8_t>((total >> 16) & 0xff);
        parity[2] = static_cast<std::uint8_t>((total >> 8) & 0xff);
        parity[3] = static_cast<std::uint8_t>(total & 0xff);
        const std::size_t begin = group * group_size;
        const std::size_t end =
            std::min(begin + group_size, chunk_count);
        for (std::size_t index = begin; index < end; ++index) {
            for (std::size_t byte = 0; byte < output.chunks[index].size(); ++byte) {
                parity[4 + byte] ^= output.chunks[index][byte];
            }
        }
        output.parity.push_back(std::move(parity));
    }
    return output;
}

gcs::video::VideoPacketHeader parityHeader(
    std::uint32_t frame_id,
    std::uint16_t group_index,
    std::uint16_t chunk_count,
    int group_size)
{
    auto output = header(
        frame_id, group_index, chunk_count,
        static_cast<std::uint32_t>(gcs::video::kVideoFecPayloadSize));
    output.flags = static_cast<std::uint16_t>(
        gcs::video::kVideoFlagFecParity |
        (static_cast<std::uint16_t>(group_size) << 8));
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

    // FEC: one lost data chunk per group is reconstructed from parity.
    {
        gcs::video::JpegFrameReassembler fec_reassembler;
        std::vector<std::uint8_t> original(4 * 1200 + 700);
        for (std::size_t i = 0; i < original.size(); ++i) {
            original[i] = static_cast<std::uint8_t>((i * 31 + 7) & 0xff);
        }
        const int group = 2; // groups {0,1} {2,3} {4}
        const auto fec = buildFecFrame(original, group);
        const auto count = static_cast<std::uint16_t>(fec.chunks.size());
        assert(count == 5);

        const std::uint32_t fid = 1000;
        const auto send_chunk = [&](std::size_t i) {
            return fec_reassembler.acceptPacket(
                header(fid, static_cast<std::uint16_t>(i), count,
                       static_cast<std::uint32_t>(fec.chunks[i].size())),
                fec.chunks[i].data(), fec.chunks[i].size());
        };
        const auto send_parity = [&](std::size_t g) {
            return fec_reassembler.acceptPacket(
                parityHeader(fid, static_cast<std::uint16_t>(g), count, group),
                fec.parity[g].data(), fec.parity[g].size());
        };

        // Lose chunk 1 (recovered by parity 0), chunk 4 — the short final
        // chunk (recovered by parity 2, needs the total byte count).
        assert(!send_chunk(0));
        assert(!send_parity(0)); // chunk 1 recovered here
        assert(!send_chunk(2));
        assert(!send_chunk(3));
        assert(!send_parity(1)); // nothing missing in group 1
        const auto recovered = send_parity(2); // chunk 4 recovered -> complete
        assert(recovered);
        assert(recovered->data == original);
        assert(fec_reassembler.stats().fec_recovered_chunks == 2);
        assert(fec_reassembler.stats().completed_frames == 1);
    }

    // FEC: two losses in one group cannot be recovered; the next frame
    // abandons the incomplete one.
    {
        gcs::video::JpegFrameReassembler fec_reassembler;
        std::vector<std::uint8_t> original(3 * 1200, 0x5a);
        const auto fec = buildFecFrame(original, 3);
        const auto count = static_cast<std::uint16_t>(fec.chunks.size());
        assert(count == 3);
        assert(!fec_reassembler.acceptPacket(
            header(2000, 0, count, 1200), fec.chunks[0].data(), 1200));
        assert(!fec_reassembler.acceptPacket(
            parityHeader(2000, 0, count, 3),
            fec.parity[0].data(), fec.parity[0].size()));
        // chunks 1 and 2 lost -> unrecoverable; a new single-chunk frame
        // abandons the incomplete one and completes on its own.
        const auto next = fec_reassembler.acceptPacket(
            header(2001, 0, 1, sizeof(a)), a, sizeof(a));
        assert(next);
        assert(fec_reassembler.stats().fec_recovered_chunks == 0);
        assert(fec_reassembler.stats().incomplete_frames == 1);
    }

    return 0;
}
