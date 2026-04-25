#include <chrono>
#include <cerrno>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

struct Options {
    std::string gcs_ip = "127.0.0.1";
    int port = 14550;
    int count = 5;
    int interval_ms = 1000;
};

void printUsage()
{
    std::cout
        << "Usage: mock_onboard [options]\n"
        << "\n"
        << "Options:\n"
        << "  --gcs-ip <ip>       Destination GCS IP, default 127.0.0.1\n"
        << "  --port <n>          Destination UDP port, default 14550\n"
        << "  --count <n>         Number of packets to send, default 5\n"
        << "  --interval-ms <n>   Send interval, default 1000\n"
        << "  -h, --help          Show this help\n";
}

int parseInt(const std::string& value, int fallback)
{
    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
    }
}

Options parseOptions(int argc, char** argv)
{
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--gcs-ip" && i + 1 < argc) {
            options.gcs_ip = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            options.port = parseInt(argv[++i], options.port);
        } else if (arg == "--count" && i + 1 < argc) {
            options.count = parseInt(argv[++i], options.count);
        } else if (arg == "--interval-ms" && i + 1 < argc) {
            options.interval_ms = parseInt(argv[++i], options.interval_ms);
        } else if (arg == "-h" || arg == "--help") {
            printUsage();
            std::exit(0);
        } else {
            std::cerr << "unknown or incomplete option: " << arg << "\n";
            printUsage();
            std::exit(2);
        }
    }
    return options;
}

long long timestampMs()
{
    const auto now = std::chrono::system_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch());
    return ms.count();
}

std::string buildTelemetryJson(int seq)
{
    std::ostringstream json;
    json << "{";
    json << "\"protocol_version\":1,";
    json << "\"type\":\"TELEMETRY\",";
    json << "\"seq\":" << seq << ",";
    json << "\"timestamp_ms\":" << timestampMs() << ",";
    json << "\"mission\":{\"state\":\"IDLE\",\"elapsed_ms\":0},";
    json << "\"grid_pose\":{\"row\":-1,\"col\":-1,\"heading_deg\":0.0},";
    json << "\"marker_map\":[],";
    json << "\"vision\":{\"line_offset\":0.0,\"line_angle\":0.0,\"intersection_score\":0.0,"
         << "\"marker_id\":-1,\"marker_offset_x\":0.0,\"marker_offset_y\":0.0},";
    json << "\"drone\":{\"altitude_m\":0.0,\"battery_voltage\":0.0,\"battery_pct\":0,"
         << "\"armed\":false,\"flight_mode\":\"UNKNOWN\",\"failsafe\":false},";
    json << "\"safety\":{\"line_lost\":false,\"gcs_link_lost\":false,"
         << "\"pixhawk_link_lost\":false,\"low_battery\":false},";
    json << "\"bringup\":{\"camera_status\":\"mock\",\"frame_width\":640,"
         << "\"frame_height\":480,\"measured_fps\":30.0,\"note\":\"mock_onboard\"}";
    json << "}";
    return json.str();
}

} // namespace

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);

#ifdef _WIN32
    WSADATA data {};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }
#endif

    const auto socket_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
#ifdef _WIN32
    if (socket_fd == INVALID_SOCKET) {
        std::cerr << "socket open failed: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }
#else
    if (socket_fd < 0) {
        std::cerr << "socket open failed: " << std::strerror(errno) << "\n";
        return 1;
    }
#endif

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<std::uint16_t>(options.port));
    if (inet_pton(AF_INET, options.gcs_ip.c_str(), &address.sin_addr) != 1) {
        std::cerr << "invalid GCS IP: " << options.gcs_ip << "\n";
        return 1;
    }

    std::cout << "Sending mock TELEMETRY to "
              << options.gcs_ip << ':' << options.port << "\n";
    for (int seq = 1; seq <= options.count; ++seq) {
        const std::string payload = buildTelemetryJson(seq);
        const int sent = sendto(
            socket_fd,
            payload.data(),
            static_cast<int>(payload.size()),
            0,
            reinterpret_cast<sockaddr*>(&address),
            sizeof(address));
        if (sent < 0) {
            std::cerr << "sendto failed\n";
            return 1;
        }
        std::cout << "sent TELEMETRY seq=" << seq << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(options.interval_ms));
    }

#ifdef _WIN32
    closesocket(socket_fd);
    WSACleanup();
#else
    close(socket_fd);
#endif

    return 0;
}
