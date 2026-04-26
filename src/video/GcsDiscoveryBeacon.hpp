#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

namespace gcs::video {

class GcsDiscoveryBeacon {
public:
    GcsDiscoveryBeacon() = default;
    ~GcsDiscoveryBeacon();

    GcsDiscoveryBeacon(const GcsDiscoveryBeacon&) = delete;
    GcsDiscoveryBeacon& operator=(const GcsDiscoveryBeacon&) = delete;

    bool start(std::uint16_t video_port, std::uint16_t discovery_port = 5601, int interval_ms = 1000);
    void stop();
    std::string lastError() const;

private:
    void run(std::uint16_t video_port, std::uint16_t discovery_port, int interval_ms);

    std::atomic_bool running_ {false};
    std::thread worker_;
    std::string last_error_;
};

} // namespace gcs::video
