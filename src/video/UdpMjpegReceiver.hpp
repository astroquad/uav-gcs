#pragma once

#include "video/JpegFrameReassembler.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace gcs::video {

class UdpMjpegReceiver {
public:
    UdpMjpegReceiver();
    ~UdpMjpegReceiver();

    UdpMjpegReceiver(const UdpMjpegReceiver&) = delete;
    UdpMjpegReceiver& operator=(const UdpMjpegReceiver&) = delete;

    bool open(std::uint16_t port);
    std::optional<JpegFrame> receiveFrame(int timeout_ms);
    void close();
    std::string lastError() const;

private:
    std::uintptr_t socket_ = 0;
    bool socket_open_ = false;
    JpegFrameReassembler reassembler_;
    mutable std::string last_error_;
};

} // namespace gcs::video
