#pragma once

#include "protocol/TelemetryMessage.hpp"

#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>

namespace gcs::telemetry {

struct GridMapCoord {
    int x = 0;
    int y = 0;
};

struct GridMapCoordLess {
    bool operator()(const GridMapCoord& lhs, const GridMapCoord& rhs) const
    {
        if (lhs.y != rhs.y) {
            return lhs.y < rhs.y;
        }
        return lhs.x < rhs.x;
    }
};

class GridMapTracker {
public:
    bool observe(const protocol::GridNodeTelemetry& node);
    // Cycle 13: update the drone fractional position used by render() to draw
    // the heading arrow at a sub-cell location between commits.
    void observeDronePosition(bool valid, double grid_offset_x, double grid_offset_y);
    // Cycle 25: live drone coord + heading from the mission state. Drives
    // the arrow direction so it updates the moment a turn completes, not
    // when the next node is committed.
    void observeMission(const protocol::MissionTelemetry& mission);
    std::string render() const;
    void reset();

    std::size_t nodeCount() const;

private:
    struct Node {
        protocol::GridNodeTelemetry telemetry;
        std::uint32_t order = 0;
    };

    struct Edge {
        GridMapCoord a;
        GridMapCoord b;
    };

    struct EdgeLess {
        bool operator()(const Edge& lhs, const Edge& rhs) const;
    };

    static GridMapCoord coordFor(const protocol::GridNodeTelemetry& node);
    static GridMapCoord startCoordFor(const protocol::GridNodeTelemetry& first_node);
    static char headingArrow(const std::string& heading);

    std::map<GridMapCoord, Node, GridMapCoordLess> nodes_;
    std::set<Edge, EdgeLess> edges_;
    std::set<std::uint32_t> observed_ids_;
    GridMapCoord current_coord_;
    GridMapCoord first_coord_;
    protocol::GridNodeTelemetry first_node_;
    bool has_current_ = false;
    bool has_first_ = false;
    std::uint32_t next_order_ = 1;
    // Cycle 13: drone fractional position from the last committed node.
    bool   has_drone_pos_ = false;
    double drone_offset_x_ = 0.0;
    double drone_offset_y_ = 0.0;
    // Cycle 25: live mission heading + drone coord (post-turn, before next
    // commit). Falls back to per-node arrival_heading if not set.
    bool has_mission_heading_ = false;
    std::string mission_heading_;       // "north"/"east"/"south"/"west"
    GridMapCoord mission_drone_coord_;
    bool has_mission_drone_coord_ = false;
    // Cycle 25: marker registry mirrored from MissionTelemetry. Used to
    // render an 'M'-style mark at marker cells in place of the generic '+'.
    std::map<GridMapCoord, int, GridMapCoordLess> marker_cells_;  // coord -> id
    bool grid_map_finalized_ = false;
    mutable std::mutex mutex_;
};

} // namespace gcs::telemetry
