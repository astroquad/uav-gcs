#pragma once

#include "network/TelemetryReassembler.hpp"

#include <cstdint>
#include <string>

namespace gcs::network {

class UdpTelemetryReceiver {
public:
    UdpTelemetryReceiver();
    ~UdpTelemetryReceiver();

    UdpTelemetryReceiver(const UdpTelemetryReceiver&) = delete;
    UdpTelemetryReceiver& operator=(const UdpTelemetryReceiver&) = delete;

    bool open(std::uint16_t port);
    // Blocks up to timeout_ms for one complete telemetry JSON message.
    // Chunked (AQT1) datagrams are reassembled transparently.
    bool receive(std::string& payload, int timeout_ms);
    std::string lastError() const;
    TelemetryReassemblerStats reassemblerStats() const;

private:
    std::uintptr_t socket_ = 0;
    bool socket_open_ = false;
    TelemetryReassembler reassembler_;
    mutable std::string last_error_;
};

} // namespace gcs::network
