#include "telemetry/MarkerTracker.hpp"

#include <iomanip>
#include <sstream>

namespace gcs::telemetry {

void MarkerTracker::observe(const protocol::MissionTelemetry& mission)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!mission.present) {
        return;
    }
    latest_ = mission;
    expected_ = mission.markers_expected;
    for (const auto& m : mission.markers_found) {
        if (m.id < 0) continue;
        // map::operator[] inserts-or-replaces. Replacing keeps the latest
        // known coord/orientation if onboard refines its position, while
        // first_seen_s should remain stable (onboard sets it once at commit).
        markers_[m.id] = m;
    }
}

std::string MarkerTracker::render() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream out;
    out << "[markers] count=" << markers_.size();
    if (expected_ > 0) {
        out << " / " << expected_;
    }
    out << "\r\n\r\n";
    if (markers_.empty()) {
        out << "(none yet)\r\n";
        return out.str();
    }
    for (const auto& [id, m] : markers_) {
        out << "id=" << id
            << "  grid=(" << std::setw(2) << m.grid_x
            << "," << std::setw(2) << m.grid_y << ")"
            << "  t=" << std::fixed << std::setprecision(1) << m.first_seen_s << "s"
            << "\r\n";
    }
    return out.str();
}

void MarkerTracker::reset()
{
    std::lock_guard<std::mutex> lock(mutex_);
    markers_.clear();
    expected_ = 0;
    latest_ = {};
}

protocol::MissionTelemetry MarkerTracker::latestMission() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return latest_;
}

std::size_t MarkerTracker::count() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return markers_.size();
}

int MarkerTracker::expected() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return expected_;
}

} // namespace gcs::telemetry
