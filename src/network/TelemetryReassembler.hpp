#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace gcs::network {

struct TelemetryReassemblerStats {
    std::uint64_t legacy_messages = 0;
    std::uint64_t chunked_messages = 0;
    std::uint64_t malformed_packets = 0;
    std::uint64_t incomplete_messages = 0;
    std::uint64_t old_packets = 0;
    std::uint64_t chunk_mismatch_resets = 0;
};

// Rebuilds telemetry JSON messages from UDP datagrams. Onboard senders emit
// bare JSON for payloads that fit one MTU-safe datagram and AQT1-framed
// chunks (video header layout, frame_id = message_id) above that. A datagram
// starting with '{' is returned immediately as a legacy/unchunked message.
// Keeps a single in-flight message; a newer message_id abandons the current
// one (mirrors video's JpegFrameReassembler).
class TelemetryReassembler {
public:
    std::optional<std::string> acceptDatagram(const std::uint8_t* data, std::size_t size);
    TelemetryReassemblerStats stats() const;

private:
    void noteIncompleteMessage();
    void reset();

    std::uint32_t current_message_id_ = 0;
    std::uint16_t expected_chunks_ = 0;
    std::uint16_t received_chunks_ = 0;
    std::vector<std::string> chunks_;
    TelemetryReassemblerStats stats_;
};

} // namespace gcs::network
