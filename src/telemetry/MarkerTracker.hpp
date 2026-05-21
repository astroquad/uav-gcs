#pragma once

#include "protocol/TelemetryMessage.hpp"

#include <map>
#include <mutex>
#include <string>

namespace gcs::telemetry {

// Cycle 23: tracks the discovered-marker registry sent by onboard and renders
// it as ASCII text for the GCS markers side panel.
//
// std::map<int, ...> keeps entries automatically sorted by ascending ID and
// deduplicates on insert in O(log n). Suitable for the panel's per-frame
// rebuild pattern. (User asked about heap; std::priority_queue would be
// natural for top-k extraction but cannot be iterated in order without
// destroying it, so it is the wrong fit here.)
class MarkerTracker {
public:
    void observe(const protocol::MissionTelemetry& mission);
    std::string render() const;
    void reset();

    std::size_t count() const;
    int expected() const;

    // Cycle 23: cache the full mission block (state + intent + flags) so
    // the detail-log formatter can lead with mission context without each
    // caller plumbing the parsed TelemetryMessage all the way down.
    protocol::MissionTelemetry latestMission() const;

private:
    mutable std::mutex mutex_;
    std::map<int, protocol::MissionMarkerEntry> markers_;
    int expected_ = 0;
    protocol::MissionTelemetry latest_;
};

} // namespace gcs::telemetry
