#include "telemetry/GridMapTracker.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <vector>

namespace gcs::telemetry {
namespace {

GridMapCoord normalizedEdgeStart(GridMapCoord a, GridMapCoord b)
{
    if (GridMapCoordLess {}(b, a)) {
        return b;
    }
    return a;
}

GridMapCoord normalizedEdgeEnd(GridMapCoord a, GridMapCoord b)
{
    if (GridMapCoordLess {}(b, a)) {
        return a;
    }
    return b;
}

GridMapCoord headingVector(const std::string& heading)
{
    // Cycle 13: reverted to screen convention — north = -y, south = +y.
    // First grid node is (0,0), subsequent nodes (0,-1), (0,-2)... up the
    // column; vertiport sits at (0,+1). canvasRow uses (y - min_y) * 2 so a
    // smaller y still renders at the top of the canvas (north is visual up).
    if (heading == "north") {
        return {0, -1};
    }
    if (heading == "east") {
        return {1, 0};
    }
    if (heading == "south") {
        return {0, 1};
    }
    if (heading == "west") {
        return {-1, 0};
    }
    return {0, -1};
}

void put(std::vector<std::string>& canvas, int row, int col, char value)
{
    if (row < 0 || col < 0 ||
        row >= static_cast<int>(canvas.size()) ||
        col >= static_cast<int>(canvas[static_cast<std::size_t>(row)].size())) {
        return;
    }
    canvas[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] = value;
}

} // namespace

bool GridMapTracker::EdgeLess::operator()(const Edge& lhs, const Edge& rhs) const
{
    const auto lhs_a = normalizedEdgeStart(lhs.a, lhs.b);
    const auto lhs_b = normalizedEdgeEnd(lhs.a, lhs.b);
    const auto rhs_a = normalizedEdgeStart(rhs.a, rhs.b);
    const auto rhs_b = normalizedEdgeEnd(rhs.a, rhs.b);
    const GridMapCoordLess less;
    if (less(lhs_a, rhs_a)) {
        return true;
    }
    if (less(rhs_a, lhs_a)) {
        return false;
    }
    return less(lhs_b, rhs_b);
}

bool GridMapTracker::observe(const protocol::GridNodeTelemetry& node)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!node.valid) {
        return false;
    }
    if (grid_map_finalized_) {
        return false;
    }
    if (node.id != 0 && observed_ids_.find(node.id) != observed_ids_.end()) {
        return false;
    }

    const GridMapCoord coord = coordFor(node);
    // Cycle 19: coord-based dedup safety net. If a different node ID arrives
    // at the same (x,y) (rare onboard race) keep the first entry rather than
    // overwriting — otherwise the visual grid grows phantom cells while the
    // old node disappears.
    auto coord_it = nodes_.find(coord);
    if (coord_it != nodes_.end() && node.id != 0 &&
        coord_it->second.telemetry.id != 0 &&
        coord_it->second.telemetry.id != node.id) {
        return false;
    }
    if (has_current_ &&
        (std::abs(coord.x - current_coord_.x) + std::abs(coord.y - current_coord_.y) == 1)) {
        edges_.insert({current_coord_, coord});
    }

    if (node.id != 0) {
        observed_ids_.insert(node.id);
    }
    auto& entry = nodes_[coord];
    entry.telemetry = node;
    if (entry.order == 0) {
        entry.order = next_order_++;
    }

    if (node.updates_current) {
        current_coord_ = coord;
        has_current_ = true;
    }
    if ((!has_first_ || node.first_node) && node.updates_current) {
        first_coord_ = coord;
        first_node_ = node;
        has_first_ = true;
    }
    return true;
}

std::string GridMapTracker::render() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (nodes_.empty()) {
        return "[grid-map] empty\n";
    }

    GridMapCoord min_coord = nodes_.begin()->first;
    GridMapCoord max_coord = nodes_.begin()->first;
    for (const auto& [coord, node] : nodes_) {
        (void)node;
        min_coord.x = std::min(min_coord.x, coord.x);
        min_coord.y = std::min(min_coord.y, coord.y);
        max_coord.x = std::max(max_coord.x, coord.x);
        max_coord.y = std::max(max_coord.y, coord.y);
    }

    // Cycle 16: start coord ('s') was removed — the new arena has no
    // vertiport->grid entry line, so the (0,0) grid origin is the very first
    // node rendered. Canvas extent is taken straight from the node set.
    const int rows = (max_coord.y - min_coord.y) * 2 + 1;
    const int cols = (max_coord.x - min_coord.x) * 4 + 1;
    std::vector<std::string> canvas(
        static_cast<std::size_t>(std::max(1, rows)),
        std::string(static_cast<std::size_t>(std::max(1, cols)), ' '));

    // Cycle 13 revert: north = -y. Smaller y must render at the TOP of the
    // canvas so the visualization still reads "north is up".
    const auto canvasRow = [&](int y) {
        return (y - min_coord.y) * 2;
    };
    const auto canvasCol = [&](int x) {
        return (x - min_coord.x) * 4;
    };

    const auto drawEdge = [&](GridMapCoord a, GridMapCoord b) {
        const int row_a = canvasRow(a.y);
        const int col_a = canvasCol(a.x);
        const int row_b = canvasRow(b.y);
        const int col_b = canvasCol(b.x);
        if (row_a == row_b) {
            const int start = std::min(col_a, col_b);
            const int end = std::max(col_a, col_b);
            for (int col = start + 1; col < end; ++col) {
                put(canvas, row_a, col, '-');
            }
        } else if (col_a == col_b) {
            const int start = std::min(row_a, row_b);
            const int end = std::max(row_a, row_b);
            for (int row = start + 1; row < end; ++row) {
                put(canvas, row, col_a, '|');
            }
        }
    };

    for (const auto& edge : edges_) {
        drawEdge(edge.a, edge.b);
    }
    for (const auto& [coord, node] : nodes_) {
        (void)node;
        const GridMapCoord east {coord.x + 1, coord.y};
        // Cycle 13 revert: north = -y, so the south neighbor sits at y + 1.
        const GridMapCoord south {coord.x, coord.y + 1};
        if (nodes_.find(east) != nodes_.end()) {
            drawEdge(coord, east);
        }
        if (nodes_.find(south) != nodes_.end()) {
            drawEdge(coord, south);
        }
    }

    for (const auto& [coord, node] : nodes_) {
        (void)node;
        put(canvas, canvasRow(coord.y), canvasCol(coord.x), '+');
    }
    // Cycle 25: stamp marker glyph on each marker cell. Use the id digit for
    // 1..9 and 'M' as a generic fallback for higher ids. This overwrites the
    // generic '+' for marker cells, then the drone arrow (below) overwrites
    // again at the current cell so the operator always sees the live arrow.
    for (const auto& [coord, id] : marker_cells_) {
        const char glyph = (id >= 1 && id <= 9) ? static_cast<char>('0' + id) : 'M';
        put(canvas, canvasRow(coord.y), canvasCol(coord.x), glyph);
    }
    // Cycle 25: drone arrow uses the live mission heading if available (so it
    // flips direction immediately after a turn completes, not at next commit).
    // Falls back to the latest committed node's arrival_heading.
    const bool use_mission_pos =
        has_mission_drone_coord_ && has_mission_heading_;
    const GridMapCoord arrow_coord = use_mission_pos
        ? mission_drone_coord_
        : (has_current_ ? current_coord_ : GridMapCoord{});
    const bool have_arrow = use_mission_pos || has_current_;
    if (have_arrow) {
        std::string heading;
        if (has_mission_heading_) {
            heading = mission_heading_;
        } else {
            const auto found = nodes_.find(arrow_coord);
            heading = (found == nodes_.end())
                ? std::string("unknown")
                : found->second.telemetry.arrival_heading;
        }
        put(canvas, canvasRow(arrow_coord.y), canvasCol(arrow_coord.x),
            headingArrow(heading));
    }

    std::ostringstream stream;
    const GridMapCoord current = arrow_coord;
    std::string heading_label;
    if (has_mission_heading_) {
        heading_label = mission_heading_;
    } else {
        const auto found = nodes_.find(current);
        heading_label = (found == nodes_.end())
            ? std::string("unknown")
            : found->second.telemetry.arrival_heading;
    }
    stream << "[grid-map] nodes=" << nodes_.size()
           << " current=(" << current.x << ',' << current.y << ")"
           << " heading=" << heading_label << "\n";
    // Cycle 27: headingArrow() reverted to plain ASCII glyphs (^ > v <),
    // so the canvas can be streamed out byte-for-byte without any UTF-8
    // substitution.
    for (const auto& row : canvas) {
        stream << row << "\n";
    }
    return stream.str();
}

void GridMapTracker::observeDronePosition(bool valid, double grid_offset_x, double grid_offset_y)
{
    std::lock_guard<std::mutex> lock(mutex_);
    has_drone_pos_ = valid;
    drone_offset_x_ = valid ? grid_offset_x : 0.0;
    drone_offset_y_ = valid ? grid_offset_y : 0.0;
}

void GridMapTracker::observeMission(const protocol::MissionTelemetry& mission)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!mission.present) {
        return;
    }
    if (mission.grid_map_finalized) {
        grid_map_finalized_ = true;
    }
    // Live heading drives the arrow direction immediately after a turn.
    if (mission.grid.valid && !mission.grid.heading.empty() &&
        mission.grid.heading != "unknown") {
        mission_heading_ = mission.grid.heading;
        has_mission_heading_ = true;
        mission_drone_coord_ = GridMapCoord{mission.grid.x, mission.grid.y};
        has_mission_drone_coord_ = true;
    }
    // Mirror discovered markers into a coord->id map so render() can stamp
    // a marker glyph at those cells. Vertiport is excluded (no grid coord
    // on that record anyway) so the start pad stays unmarked.
    const int vertiport_id = mission.vertiport.marker_id;
    for (const auto& m : mission.markers_found) {
        if (!m.grid_valid) continue;
        if (m.id == vertiport_id) continue;
        marker_cells_[GridMapCoord{m.grid_x, m.grid_y}] = m.id;
    }
}

void GridMapTracker::reset()
{
    std::lock_guard<std::mutex> lock(mutex_);
    nodes_.clear();
    edges_.clear();
    observed_ids_.clear();
    current_coord_ = {};
    first_coord_ = {};
    first_node_ = {};
    has_current_ = false;
    has_first_ = false;
    next_order_ = 1;
    has_drone_pos_ = false;
    drone_offset_x_ = 0.0;
    drone_offset_y_ = 0.0;
    has_mission_heading_ = false;
    mission_heading_.clear();
    has_mission_drone_coord_ = false;
    mission_drone_coord_ = {};
    marker_cells_.clear();
    grid_map_finalized_ = false;
}

std::size_t GridMapTracker::nodeCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return nodes_.size();
}

GridMapCoord GridMapTracker::coordFor(const protocol::GridNodeTelemetry& node)
{
    return {node.x, node.y};
}

GridMapCoord GridMapTracker::startCoordFor(const protocol::GridNodeTelemetry& first_node)
{
    const auto vector = headingVector(first_node.arrival_heading);
    return {
        first_node.x - vector.x,
        first_node.y - vector.y,
    };
}

char GridMapTracker::headingArrow(const std::string& heading)
{
    // Cycle 27: reverted to plain ASCII arrows — the Unicode triangle
    // glyphs (Cycle 25) traded uniform shape for worse legibility against
    // the surrounding '+' / line characters in the fixed-width font.
    if (heading == "north") return '^';
    if (heading == "east")  return '>';
    if (heading == "south") return 'v';
    if (heading == "west")  return '<';
    return '@';
}

} // namespace gcs::telemetry
