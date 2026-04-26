#include "video/GcsDiscoveryBeacon.hpp"

#include <chrono>
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

GcsDiscoveryBeacon::~GcsDiscoveryBeacon()
{
    stop();
}

bool GcsDiscoveryBeacon::start(std::uint16_t video_port, std::uint16_t discovery_port, int interval_ms)
{
    if (running_) {
        return true;
    }
    running_ = true;
    worker_ = std::thread(&GcsDiscoveryBeacon::run, this, video_port, discovery_port, interval_ms);
    return true;
}

void GcsDiscoveryBeacon::stop()
{
    running_ = false;
    if (worker_.joinable()) {
        worker_.join();
    }
}

std::string GcsDiscoveryBeacon::lastError() const
{
    return last_error_;
}

void GcsDiscoveryBeacon::run(std::uint16_t video_port, std::uint16_t discovery_port, int interval_ms)
{
#ifdef _WIN32
    WSADATA data {};
    WSAStartup(MAKEWORD(2, 2), &data);
#endif

    const auto raw_socket = socket(AF_INET, SOCK_DGRAM, 0);
#ifdef _WIN32
    if (raw_socket == INVALID_SOCKET) {
#else
    if (raw_socket < 0) {
#endif
        last_error_ = socketErrorString();
        running_ = false;
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }
    const auto socket_handle = static_cast<std::uintptr_t>(raw_socket);

    const int enable_broadcast = 1;
    setsockopt(
#ifdef _WIN32
        static_cast<SOCKET>(socket_handle),
        SOL_SOCKET,
        SO_BROADCAST,
        reinterpret_cast<const char*>(&enable_broadcast),
#else
        static_cast<int>(socket_handle),
        SOL_SOCKET,
        SO_BROADCAST,
        &enable_broadcast,
#endif
        sizeof(enable_broadcast));

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_port = htons(discovery_port);
    address.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    const std::string message = "AQGCS1 video_port=" + std::to_string(video_port) + "\n";
    const auto interval = std::chrono::milliseconds(interval_ms > 0 ? interval_ms : 1000);
    while (running_) {
        const auto sent = sendto(
#ifdef _WIN32
            static_cast<SOCKET>(socket_handle),
#else
            static_cast<int>(socket_handle),
#endif
            message.data(),
            static_cast<int>(message.size()),
            0,
            reinterpret_cast<sockaddr*>(&address),
            sizeof(address));
        if (sent < 0) {
            last_error_ = socketErrorString();
        } else {
            last_error_.clear();
        }
        std::this_thread::sleep_for(interval);
    }

    closeSocket(socket_handle);
#ifdef _WIN32
    WSACleanup();
#endif
}

} // namespace gcs::video
