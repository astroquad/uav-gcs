#include "app/TelemetryWorker.hpp"

#include "telemetry/GridMapTracker.hpp"
#include "telemetry/MarkerTracker.hpp"
#include "telemetry/TelemetryStore.hpp"

#include <algorithm>
#include <utility>

namespace gcs::app {

TelemetryWorker::~TelemetryWorker()
{
    stop();
}

bool TelemetryWorker::start(
    std::uint16_t port,
    int timeout_ms,
    telemetry::TelemetryStore& store,
    telemetry::GridMapTracker& grid_map,
    telemetry::MarkerTracker& marker_tracker)
{
    if (!receiver_.open(port)) {
        last_error_ = receiver_.lastError();
        return false;
    }

    running_ = true;
    worker_ = std::thread([this, timeout_ms, &store, &grid_map, &marker_tracker]() {
        while (running_) {
            std::string payload;
            if (!receiver_.receive(payload, std::clamp(timeout_ms, 1, 100))) {
                if (receiver_.lastError() != "timeout") {
                    std::lock_guard<std::mutex> lock(error_mutex_);
                    last_error_ = receiver_.lastError();
                }
                continue;
            }

            const auto parsed = protocol::parseTelemetryJson(payload);
            if (!parsed) {
                std::lock_guard<std::mutex> lock(error_mutex_);
                last_error_ = "dropped malformed telemetry JSON";
                continue;
            }

            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.observe(*parsed);
            }
            store.observe(*parsed);
            // Mission may finalize the map at marker revisit. Apply mission
            // state before grid_node so one packet cannot add after freeze.
            grid_map.observeMission(parsed->mission);
            grid_map.observe(parsed->vision.grid_node);
            grid_map.observeDronePosition(
                parsed->vision.drone_position.valid,
                parsed->vision.drone_position.grid_offset_x,
                parsed->vision.drone_position.grid_offset_y);
            marker_tracker.observe(parsed->mission);
        }
    });
    return true;
}

void TelemetryWorker::stop()
{
    running_ = false;
    if (worker_.joinable()) {
        worker_.join();
    }
}

protocol::TelemetryStats TelemetryWorker::stats() const
{
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

std::string TelemetryWorker::takeLastError()
{
    std::lock_guard<std::mutex> lock(error_mutex_);
    std::string output = std::move(last_error_);
    last_error_.clear();
    return output;
}

} // namespace gcs::app
