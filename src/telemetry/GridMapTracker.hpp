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
    mutable std::mutex mutex_;
};

} // namespace gcs::telemetry
