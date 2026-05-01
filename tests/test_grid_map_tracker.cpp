#include "telemetry/GridMapTracker.hpp"

#include <cassert>
#include <string>

namespace {

gcs::protocol::GridNodeTelemetry node(
    std::uint32_t id,
    int x,
    int y,
    const std::string& heading,
    bool first = false)
{
    gcs::protocol::GridNodeTelemetry output;
    output.valid = true;
    output.id = id;
    output.x = x;
    output.y = y;
    output.arrival_heading = heading;
    output.topology = first ? "L" : "T";
    output.first_node = first;
    return output;
}

} // namespace

int main()
{
    gcs::telemetry::GridMapTracker tracker;
    assert(tracker.observe(node(1, 0, 0, "north", true)));
    std::string text = tracker.render();
    assert(text.find('s') != std::string::npos);
    assert(text.find('^') != std::string::npos);
    assert(tracker.nodeCount() == 1);

    assert(tracker.observe(node(2, 1, 0, "east")));
    assert(tracker.observe(node(3, 2, 0, "east")));
    assert(!tracker.observe(node(3, 2, 0, "east")));
    text = tracker.render();
    assert(text.find("+---+--->") != std::string::npos);
    assert(tracker.nodeCount() == 3);

    assert(tracker.observe(node(4, 2, 1, "south")));
    assert(tracker.observe(node(5, 1, 1, "west")));
    text = tracker.render();
    assert(text.find('|') != std::string::npos);
    assert(text.find('<') != std::string::npos);
    assert(tracker.nodeCount() == 5);

    assert(tracker.observe(node(6, -1, 1, "west")));
    text = tracker.render();
    assert(text.find("current=(-1,1)") != std::string::npos);
    return 0;
}
