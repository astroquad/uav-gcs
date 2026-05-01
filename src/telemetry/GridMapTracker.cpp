#include "telemetry/GridMapTracker.hpp"

#include <algorithm>
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
    return {0, 1};
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
    if (node.id != 0 && observed_ids_.find(node.id) != observed_ids_.end()) {
        return false;
    }

    const GridMapCoord coord = coordFor(node);
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

    current_coord_ = coord;
    has_current_ = true;
    if (!has_first_ || node.first_node) {
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

    const GridMapCoord start_coord = has_first_ ? startCoordFor(first_node_) : GridMapCoord {0, 1};
    min_coord.x = std::min(min_coord.x, start_coord.x);
    min_coord.y = std::min(min_coord.y, start_coord.y);
    max_coord.x = std::max(max_coord.x, start_coord.x);
    max_coord.y = std::max(max_coord.y, start_coord.y);

    const int rows = (max_coord.y - min_coord.y) * 2 + 1;
    const int cols = (max_coord.x - min_coord.x) * 4 + 1;
    std::vector<std::string> canvas(
        static_cast<std::size_t>(std::max(1, rows)),
        std::string(static_cast<std::size_t>(std::max(1, cols)), ' '));

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
        const GridMapCoord south {coord.x, coord.y + 1};
        if (nodes_.find(east) != nodes_.end()) {
            drawEdge(coord, east);
        }
        if (nodes_.find(south) != nodes_.end()) {
            drawEdge(coord, south);
        }
    }

    if (has_first_) {
        const int first_row = canvasRow(first_coord_.y);
        const int first_col = canvasCol(first_coord_.x);
        const int start_row = canvasRow(start_coord.y);
        const int start_col = canvasCol(start_coord.x);
        if (first_row == start_row) {
            const int start = std::min(first_col, start_col);
            const int end = std::max(first_col, start_col);
            for (int col = start + 1; col < end; ++col) {
                put(canvas, first_row, col, '-');
            }
        } else if (first_col == start_col) {
            const int start = std::min(first_row, start_row);
            const int end = std::max(first_row, start_row);
            for (int row = start + 1; row < end; ++row) {
                put(canvas, row, first_col, '|');
            }
        }
        put(canvas, start_row, start_col, 's');
    }

    for (const auto& [coord, node] : nodes_) {
        (void)node;
        put(canvas, canvasRow(coord.y), canvasCol(coord.x), '+');
    }
    if (has_current_) {
        const auto found = nodes_.find(current_coord_);
        const std::string heading = found == nodes_.end()
            ? std::string("unknown")
            : found->second.telemetry.arrival_heading;
        put(canvas, canvasRow(current_coord_.y), canvasCol(current_coord_.x), headingArrow(heading));
    }

    std::ostringstream stream;
    const auto current = has_current_ ? current_coord_ : GridMapCoord {};
    const auto found = nodes_.find(current);
    const std::string heading = found == nodes_.end()
        ? std::string("unknown")
        : found->second.telemetry.arrival_heading;
    stream << "[grid-map] nodes=" << nodes_.size()
           << " current=(" << current.x << ',' << current.y << ")"
           << " heading=" << heading << "\n";
    for (const auto& row : canvas) {
        stream << row << "\n";
    }
    return stream.str();
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
    if (heading == "north") {
        return '^';
    }
    if (heading == "east") {
        return '>';
    }
    if (heading == "south") {
        return 'v';
    }
    if (heading == "west") {
        return '<';
    }
    return '@';
}

} // namespace gcs::telemetry
