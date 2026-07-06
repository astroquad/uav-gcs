// Tests are assert-based: keep assert() active even in Release
// builds (CMake adds -DNDEBUG there, which silently no-ops all checks).
#undef NDEBUG

#include "network/TelemetryReassembler.hpp"
#include "video/VideoPacket.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace {

std::string makePayload(std::size_t size)
{
    std::string payload = "{\"seq\":1,";
    payload.append(size - payload.size() - 1, 'x');
    payload.push_back('}');
    return payload;
}

// Builds AQT1 datagrams with the exact onboard wire layout.
std::vector<std::string> buildChunkedDatagrams(
    const std::string& payload,
    std::uint32_t message_id,
    std::uint64_t timestamp_ms)
{
    using namespace gcs::video;
    const std::size_t chunk_count =
        (payload.size() + kTelemetryMaxPayloadSize - 1) / kTelemetryMaxPayloadSize;
    std::vector<std::string> datagrams;
    for (std::size_t index = 0; index < chunk_count; ++index) {
        const std::size_t offset = index * kTelemetryMaxPayloadSize;
        const std::size_t chunk_size =
            std::min(kTelemetryMaxPayloadSize, payload.size() - offset);
        VideoPacketHeader header;
        header.frame_id = message_id;
        header.chunk_index = static_cast<std::uint16_t>(index);
        header.chunk_count = static_cast<std::uint16_t>(chunk_count);
        header.payload_size = static_cast<std::uint32_t>(chunk_size);
        header.timestamp_ms = timestamp_ms;
        const auto header_bytes = serializeHeader(header, kTelemetryMagic);
        std::string datagram(reinterpret_cast<const char*>(header_bytes.data()),
                             header_bytes.size());
        datagram.append(payload, offset, chunk_size);
        datagrams.push_back(std::move(datagram));
    }
    return datagrams;
}

std::optional<std::string> accept(
    gcs::network::TelemetryReassembler& reassembler,
    const std::string& datagram)
{
    return reassembler.acceptDatagram(
        reinterpret_cast<const std::uint8_t*>(datagram.data()), datagram.size());
}

} // namespace

int main()
{
    using gcs::network::TelemetryReassembler;

    // Legacy bare JSON is returned immediately.
    {
        TelemetryReassembler reassembler;
        const std::string payload = "{\"a\":1}";
        const auto message = accept(reassembler, payload);
        assert(message);
        assert(*message == payload);
        assert(reassembler.stats().legacy_messages == 1);
        assert(reassembler.stats().chunked_messages == 0);
    }

    // Three chunks in order complete a message byte-identical to the input.
    {
        TelemetryReassembler reassembler;
        const std::string payload = makePayload(3500);
        const auto datagrams = buildChunkedDatagrams(payload, 5, 999);
        assert(datagrams.size() == 3);
        assert(!accept(reassembler, datagrams[0]));
        assert(!accept(reassembler, datagrams[1]));
        const auto message = accept(reassembler, datagrams[2]);
        assert(message);
        assert(*message == payload);
        assert(reassembler.stats().chunked_messages == 1);
        assert(reassembler.stats().incomplete_messages == 0);
    }

    // A newer message id abandons the current partial message.
    {
        TelemetryReassembler reassembler;
        const std::string payload_a = makePayload(3000);
        const std::string payload_b = makePayload(1400);
        const auto message_a = buildChunkedDatagrams(payload_a, 10, 1);
        const auto message_b = buildChunkedDatagrams(payload_b, 11, 2);
        assert(!accept(reassembler, message_a[0]));
        assert(!accept(reassembler, message_b[0]));
        const auto message = accept(reassembler, message_b[1]);
        assert(message);
        assert(*message == payload_b);
        assert(reassembler.stats().incomplete_messages == 1);

        // After a completion the reassembler resets, so a late chunk from
        // the abandoned message starts a new partial (not counted old) and
        // is abandoned again by the next newer message.
        assert(!accept(reassembler, message_a[1]));
        assert(reassembler.stats().old_packets == 0);
        const auto message_c = buildChunkedDatagrams(makePayload(1400), 12, 3);
        assert(!accept(reassembler, message_c[0]));
        const auto completed_c = accept(reassembler, message_c[1]);
        assert(completed_c);
        assert(reassembler.stats().incomplete_messages == 2);
    }

    // Malformed datagrams are rejected and counted.
    {
        TelemetryReassembler reassembler;
        const std::string payload = makePayload(2000);
        auto datagrams = buildChunkedDatagrams(payload, 20, 3);

        // Wrong magic (a video packet must not be consumed as telemetry).
        std::string video_magic = datagrams[0];
        video_magic[2] = 'V';
        assert(!accept(reassembler, video_magic));

        // Truncated header.
        assert(!accept(reassembler, datagrams[0].substr(0, 10)));

        // payload_size mismatch (datagram shorter than the header claims).
        assert(!accept(
            reassembler, datagrams[0].substr(0, datagrams[0].size() - 4)));

        assert(reassembler.stats().malformed_packets == 3);
        assert(reassembler.stats().chunked_messages == 0);

        // The stream recovers with intact datagrams afterwards.
        assert(!accept(reassembler, datagrams[0]));
        const auto message = accept(reassembler, datagrams[1]);
        assert(message);
        assert(*message == payload);
    }

    // Sender restart: a large message_id regression while a partial message
    // is pending starts a new stream instead of rejecting chunks forever.
    {
        TelemetryReassembler reassembler;
        const std::string payload = makePayload(1400);
        const auto before = buildChunkedDatagrams(makePayload(2000), 500000, 1);
        const auto after = buildChunkedDatagrams(payload, 3, 2);
        assert(!accept(reassembler, before[0]));
        assert(!accept(reassembler, after[0]));
        const auto message = accept(reassembler, after[1]);
        assert(message);
        assert(*message == payload);
        assert(reassembler.stats().sender_restarts == 1);
    }

    return 0;
}
