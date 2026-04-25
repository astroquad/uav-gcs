#include "common/NetworkConfig.hpp"
#include "network/UdpTelemetryReceiver.hpp"
#include "protocol/TelemetryMessage.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

struct Options {
    std::string config_dir = "../config";
    int count = 0;
    int timeout_ms = 0;
    bool print_raw = false;
};

void printUsage()
{
    std::cout
        << "Usage: uav_gcs [options]\n"
        << "\n"
        << "Options:\n"
        << "  --config <dir>       Config directory containing network.toml\n"
        << "  --count <n>          Receive n telemetry packets, 0 means forever\n"
        << "  --timeout-ms <n>     Override telemetry receive timeout\n"
        << "  --raw                Print raw JSON payloads\n"
        << "  -h, --help           Show this help\n";
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
        if (arg == "--config" && i + 1 < argc) {
            options.config_dir = argv[++i];
        } else if (arg == "--count" && i + 1 < argc) {
            options.count = parseInt(argv[++i], options.count);
        } else if (arg == "--timeout-ms" && i + 1 < argc) {
            options.timeout_ms = parseInt(argv[++i], options.timeout_ms);
        } else if (arg == "--raw") {
            options.print_raw = true;
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

} // namespace

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);
    auto config = gcs::common::loadNetworkConfig(options.config_dir);
    if (options.timeout_ms > 0) {
        config.telemetry_timeout_ms = options.timeout_ms;
    }

    gcs::network::UdpTelemetryReceiver receiver;
    if (!receiver.open(config.telemetry_port)) {
        std::cerr << "failed to open telemetry receiver on UDP port "
                  << config.telemetry_port << ": " << receiver.lastError() << "\n";
        return 1;
    }

    std::cout << "uav_gcs telemetry receiver\n"
              << "  listen UDP port: " << config.telemetry_port << "\n"
              << "  timeout_ms: " << config.telemetry_timeout_ms << "\n"
              << "  count: " << (options.count == 0 ? std::string("forever") : std::to_string(options.count))
              << "\n";

    int received_count = 0;
    while (options.count == 0 || received_count < options.count) {
        std::string payload;
        if (!receiver.receive(payload, config.telemetry_timeout_ms)) {
            if (receiver.lastError() == "timeout") {
                std::cout << "telemetry timeout after "
                          << config.telemetry_timeout_ms << " ms\n";
                continue;
            }
            std::cerr << "telemetry receive failed: " << receiver.lastError() << "\n";
            return 1;
        }

        const auto message = gcs::protocol::parseTelemetryJson(payload);
        if (!message) {
            std::cerr << "dropped malformed telemetry payload\n";
            if (options.print_raw) {
                std::cerr << payload << "\n";
            }
            continue;
        }

        std::cout << "TELEMETRY"
                  << " seq=" << message->seq
                  << " timestamp_ms=" << message->timestamp_ms
                  << " state=" << (message->mission_state.empty() ? "UNKNOWN" : message->mission_state)
                  << " camera=" << (message->camera_status.empty() ? "UNKNOWN" : message->camera_status)
                  << " frame=" << message->frame_width << "x" << message->frame_height
                  << " fps=" << message->measured_fps
                  << "\n";
        if (options.print_raw) {
            std::cout << payload << "\n";
        }
        ++received_count;
    }

    return 0;
}
