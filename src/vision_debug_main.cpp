#include "app/VisionDebugApp.hpp"
#include "common/NetworkConfig.hpp"

#include <toml++/toml.hpp>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

struct Options {
    std::string config_dir = "config";
    int video_port_override = 0;
    int telemetry_port_override = 0;
    int video_timeout_ms = 0;
    int marker_log_interval_ms = 0;
};

void printUsage()
{
    std::cout
        << "Usage: uav_gcs_vision_debug [options]\n\n"
        << "Options:\n"
        << "  --config <dir>          Config directory containing network.toml/ui.toml\n"
        << "  --video-port <n>        Override video UDP port\n"
        << "  --telemetry-port <n>    Override telemetry UDP port\n"
        << "  --timeout-ms <n>        Override video receive timeout\n"
        << "  --marker-log-ms <n>     Override marker log print interval\n"
        << "  -h, --help              Show this help\n";
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
        } else if (arg == "--video-port" && i + 1 < argc) {
            options.video_port_override = parseInt(argv[++i], options.video_port_override);
        } else if (arg == "--telemetry-port" && i + 1 < argc) {
            options.telemetry_port_override = parseInt(argv[++i], options.telemetry_port_override);
        } else if (arg == "--timeout-ms" && i + 1 < argc) {
            options.video_timeout_ms = parseInt(argv[++i], options.video_timeout_ms);
        } else if (arg == "--marker-log-ms" && i + 1 < argc) {
            options.marker_log_interval_ms = parseInt(argv[++i], options.marker_log_interval_ms);
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

std::string joinConfigPath(const std::string& config_dir, const std::string& filename)
{
    if (config_dir.empty()) {
        return "config/" + filename;
    }
    const char last = config_dir.back();
    if (last == '/' || last == '\\') {
        return config_dir + filename;
    }
    return config_dir + "/" + filename;
}

void loadUiConfig(const std::string& config_dir, gcs::app::VisionDebugOptions& options)
{
    try {
        const auto table = toml::parse_file(joinConfigPath(config_dir, "ui.toml"));
        if (const auto video_window = table["video_window"]) {
            options.title = video_window["title"].value_or(options.title);
            options.video_timeout_ms =
                video_window["timeout_ms"].value_or(options.video_timeout_ms);
        }
    } catch (const toml::parse_error&) {
    }
}

} // namespace

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);
    const auto network_config = gcs::common::loadNetworkConfig(options.config_dir);

    gcs::app::VisionDebugOptions app_options;
    app_options.video_port = network_config.video_port;
    app_options.telemetry_port = network_config.telemetry_port;
    app_options.telemetry_timeout_ms = network_config.telemetry_timeout_ms;
    loadUiConfig(options.config_dir, app_options);

    if (options.video_port_override > 0) {
        app_options.video_port = static_cast<std::uint16_t>(options.video_port_override);
    }
    if (options.telemetry_port_override > 0) {
        app_options.telemetry_port = static_cast<std::uint16_t>(options.telemetry_port_override);
    }
    if (options.video_timeout_ms > 0) {
        app_options.video_timeout_ms = options.video_timeout_ms;
    }
    if (options.marker_log_interval_ms > 0) {
        app_options.marker_log_interval_ms = options.marker_log_interval_ms;
    }

    gcs::app::VisionDebugApp app;
    return app.run(app_options);
}
