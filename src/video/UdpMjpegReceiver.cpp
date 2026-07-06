#include "video/UdpMjpegReceiver.hpp"

#include "video/VideoPacket.hpp"

#include <array>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace gcs::video {
namespace {

#ifdef _WIN32
std::string socketErrorString()
{
    return "winsock error " + std::to_string(WSAGetLastError());
}

void closeSocket(std::uintptr_t socket)
{
    closesocket(static_cast<SOCKET>(socket));
}
#else
std::string socketErrorString()
{
    return std::strerror(errno);
}

void closeSocket(std::uintptr_t socket)
{
    close(static_cast<int>(socket));
}
#endif

} // namespace

UdpMjpegReceiver::UdpMjpegReceiver()
{
#ifdef _WIN32
    WSADATA data {};
    WSAStartup(MAKEWORD(2, 2), &data);
#endif
}

UdpMjpegReceiver::~UdpMjpegReceiver()
{
    close();
#ifdef _WIN32
    WSACleanup();
#endif
}

bool UdpMjpegReceiver::open(std::uint16_t port)
{
    const auto raw_socket = ::socket(AF_INET, SOCK_DGRAM, 0);
#ifdef _WIN32
    if (raw_socket == INVALID_SOCKET) {
#else
    if (raw_socket < 0) {
#endif
        last_error_ = socketErrorString();
        return false;
    }
    socket_ = static_cast<std::uintptr_t>(raw_socket);

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    if (bind(
#ifdef _WIN32
            static_cast<SOCKET>(socket_),
#else
            static_cast<int>(socket_),
#endif
            reinterpret_cast<sockaddr*>(&address),
            sizeof(address)) < 0) {
        last_error_ = socketErrorString();
        closeSocket(socket_);
        socket_ = 0;
        return false;
    }

    // 24fps video arrives as ~500 chunk datagrams/s in per-frame bursts;
    // the OS default receive buffer (64KB on Windows) holds barely two
    // frames, so a brief consumer stall silently drops chunks. Best-effort:
    // a failed enlargement is not fatal.
    const int receive_buffer_bytes = 1 << 20;
    (void)setsockopt(
#ifdef _WIN32
        static_cast<SOCKET>(socket_),
        SOL_SOCKET,
        SO_RCVBUF,
        reinterpret_cast<const char*>(&receive_buffer_bytes),
#else
        static_cast<int>(socket_),
        SOL_SOCKET,
        SO_RCVBUF,
        &receive_buffer_bytes,
#endif
        sizeof(receive_buffer_bytes));

    socket_open_ = true;
    return true;
}

std::optional<JpegFrame> UdpMjpegReceiver::receiveFrame(int timeout_ms)
{
    if (!socket_open_) {
        last_error_ = "socket is not open";
        return std::nullopt;
    }

    const auto deadline_timeout_ms = timeout_ms > 0 ? timeout_ms : 1;
    while (true) {
        fd_set read_set;
        FD_ZERO(&read_set);
#ifdef _WIN32
        FD_SET(static_cast<SOCKET>(socket_), &read_set);
#else
        FD_SET(static_cast<int>(socket_), &read_set);
#endif

        timeval timeout {};
        timeout.tv_sec = deadline_timeout_ms / 1000;
        timeout.tv_usec = (deadline_timeout_ms % 1000) * 1000;

        const int ready = select(
            static_cast<int>(socket_) + 1,
            &read_set,
            nullptr,
            nullptr,
            &timeout);
        if (ready == 0) {
            last_error_ = "timeout";
            return std::nullopt;
        }
        if (ready < 0) {
            last_error_ = socketErrorString();
            return std::nullopt;
        }

        // Sized for the largest datagram: an FEC parity packet.
        std::array<std::uint8_t, kVideoHeaderSize + kVideoFecPayloadSize> packet {};
        const int received = recvfrom(
#ifdef _WIN32
            static_cast<SOCKET>(socket_),
#else
            static_cast<int>(socket_),
#endif
            reinterpret_cast<char*>(packet.data()),
            static_cast<int>(packet.size()),
            0,
            nullptr,
            nullptr);
        if (received < 0) {
            last_error_ = socketErrorString();
            return std::nullopt;
        }
        ++packets_received_;

        VideoPacketHeader header;
        if (!parseHeader(packet.data(), static_cast<std::size_t>(received), header)) {
            ++malformed_packets_;
            last_error_ = "dropped malformed video packet";
            continue;
        }

        auto frame = reassembler_.acceptPacket(
            header,
            packet.data() + kVideoHeaderSize,
            static_cast<std::size_t>(received) - kVideoHeaderSize);
        if (frame) {
            last_error_.clear();
            return frame;
        }
    }
}

void UdpMjpegReceiver::close()
{
    if (socket_open_) {
        closeSocket(socket_);
        socket_open_ = false;
    }
}

std::string UdpMjpegReceiver::lastError() const
{
    return last_error_;
}

UdpMjpegReceiverStats UdpMjpegReceiver::stats() const
{
    const auto reassembler_stats = reassembler_.stats();
    UdpMjpegReceiverStats output;
    output.packets_received = packets_received_;
    output.malformed_packets = malformed_packets_;
    output.completed_frames = reassembler_stats.completed_frames;
    output.incomplete_frames = reassembler_stats.incomplete_frames;
    output.old_packets = reassembler_stats.old_packets;
    output.chunk_mismatch_resets = reassembler_stats.chunk_mismatch_resets;
    output.sender_restarts = reassembler_stats.sender_restarts;
    output.fec_recovered_chunks = reassembler_stats.fec_recovered_chunks;
    output.last_chunk_count = reassembler_stats.last_chunk_count;
    output.last_frame_bytes = reassembler_stats.last_frame_bytes;
    return output;
}

} // namespace gcs::video
