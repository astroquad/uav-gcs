#pragma once

#include "network/UdpTelemetryReceiver.hpp"
#include "protocol/TelemetryMessage.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace gcs::telemetry {
class GridMapTracker;
class MarkerTracker;
class TelemetryStore;
} // namespace gcs::telemetry

namespace gcs::app {

class TelemetryWorker {
public:
    TelemetryWorker() = default;
    ~TelemetryWorker();

    TelemetryWorker(const TelemetryWorker&) = delete;
    TelemetryWorker& operator=(const TelemetryWorker&) = delete;

    bool start(
        std::uint16_t port,
        int timeout_ms,
        telemetry::TelemetryStore& store,
        telemetry::GridMapTracker& grid_map,
        telemetry::MarkerTracker& marker_tracker);
    void stop();

    protocol::TelemetryStats stats() const;
    std::string takeLastError();

private:
    network::UdpTelemetryReceiver receiver_;
    std::atomic<bool> running_ {false};
    std::thread worker_;
    mutable std::mutex stats_mutex_;
    protocol::TelemetryStats stats_;
    std::mutex error_mutex_;
    std::string last_error_;
};

} // namespace gcs::app
