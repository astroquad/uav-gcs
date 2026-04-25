#pragma once

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
    bool receive(std::string& payload, int timeout_ms);
    std::string lastError() const;

private:
    std::uintptr_t socket_ = 0;
    bool socket_open_ = false;
    mutable std::string last_error_;
};

} // namespace gcs::network
