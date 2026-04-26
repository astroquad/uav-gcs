#include "app/VideoViewerApp.hpp"
#include "common/NetworkConfig.hpp"

#include <toml++/toml.hpp>

#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <string>

namespace {

struct Options {
    std::string config_dir = "config";
    int port_override = 0;
    int timeout_ms = 0;
};

void printUsage()
{
    std::cout
        << "Usage: uav_gcs_video [options]\n\n"
        << "Options:\n"
        << "  --config <dir>       Config directory containing network.toml/ui.toml\n"
        << "  --port <n>           Override video UDP port\n"
        << "  --timeout-ms <n>     Override video receive timeout\n"
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
        } else if (arg == "--port" && i + 1 < argc) {
            options.port_override = parseInt(argv[++i], options.port_override);
        } else if (arg == "--timeout-ms" && i + 1 < argc) {
            options.timeout_ms = parseInt(argv[++i], options.timeout_ms);
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

void loadUiConfig(const std::string& config_dir, gcs::app::VideoViewerOptions& options)
{
    try {
        const auto table = toml::parse_file(joinConfigPath(config_dir, "ui.toml"));
        if (const auto video_window = table["video_window"]) {
            options.title = video_window["title"].value_or(options.title);
            options.timeout_ms = video_window["timeout_ms"].value_or(options.timeout_ms);
        }
    } catch (const toml::parse_error&) {
    }
}

} // namespace

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);
    const auto network_config = gcs::common::loadNetworkConfig(options.config_dir);

    gcs::app::VideoViewerOptions viewer_options;
    viewer_options.port = network_config.video_port;
    loadUiConfig(options.config_dir, viewer_options);

    if (options.port_override > 0) {
        viewer_options.port = static_cast<std::uint16_t>(options.port_override);
    }
    if (options.timeout_ms > 0) {
        viewer_options.timeout_ms = options.timeout_ms;
    }

    gcs::app::VideoViewerApp app;
    return app.run(viewer_options);
}
