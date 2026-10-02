#include "konbini/sim/pedestrian_route.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <optional>
#include <queue>
#include <stdexcept>
#include <utility>

// @implements spec/feature/npc-conversations-and-placement-feedback.md Pedestrian path walking

namespace konbini::sim {

namespace {

constexpr std::uint64_t kUnvisited = std::numeric_limits<std::uint64_t>::max();
constexpr std::uint32_t kNoEdge = std::numeric_limits<std::uint32_t>::max();

struct SearchResult {
    std::vector<std::uint64_t> costs;
    std::vector<std::uint32_t> previousEdges;
};

// Dijkstra over integer millimetre costs. On an exact cost tie the incoming
// edge with the smaller stable key wins. Every predecessor of a node has a
// strictly smaller cost (edge costs are >= 1 mm), so all tied relaxations
// happen before the node is settled and heap order cannot change the result.
[[nodiscard]] SearchResult searchFrom(const PedestrianPathTable& table,
                                      const std::size_t source) {
    SearchResult result{
        .costs = std::vector<std::uint64_t>(table.nodeCount(), kUnvisited),
        .previousEdges = std::vector<std::uint32_t>(table.nodeCount(), kNoEdge),
    };
    using Entry = std::pair<std::uint64_t, std::size_t>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<>> frontier;
    result.costs[source] = 0;
    frontier.emplace(0, source);
    while (!frontier.empty()) {
        const auto [cost, node] = frontier.top();
        frontier.pop();
        if (cost != result.costs[node]) {
            continue;
        }
        for (const std::uint32_t edge : table.adjacentEdges(node)) {
            const std::size_t next = table.edgeFromNode(edge) == node
                ? table.edgeToNode(edge)
                : table.edgeFromNode(edge);
            const std::uint64_t edgeCost = table.edgeCostMillimeters(edge);
            if (cost > kUnvisited - 1 - edgeCost) {
                throw std::overflow_error("pedestrian route cost overflow");
            }
            const std::uint64_t candidate = cost + edgeCost;
            const std::uint32_t current = result.previousEdges[next];
            if (candidate < result.costs[next]) {
                result.costs[next] = candidate;
                result.previousEdges[next] = edge;
                frontier.emplace(candidate, next);
            } else if (candidate == result.costs[next] && current != kNoEdge &&
                       table.edgeKey(edge) < table.edgeKey(current)) {
                result.previousEdges[next] = edge;
            }
        }
    }
    return result;
}

void appendWaypoint(std::vector<Vec3>& waypoints, const Vec3 point) {
    if (!waypoints.empty() && waypoints.back().x == point.x &&
        waypoints.back().z == point.z) {
        return;
    }
    waypoints.push_back(point);
}

[[nodiscard]] double planarLength(const std::vector<Vec3>& waypoints) {
    double length = 0.0;
    for (std::size_t index = 1; index < waypoints.size(); ++index) {
        length += std::hypot(waypoints[index].x - waypoints[index - 1].x,
                             waypoints[index].z - waypoints[index - 1].z);
    }
    return length;
}

}  // namespace

// @implements spec/feature/npc-conversations-and-placement-feedback.md Pedestrian path walking
PedestrianRoute selectPedestrianRoute(const PedestrianPathTable& table,
                                      const FigmentumFacilityKey from,
                                      const FigmentumFacilityKey to) {
    const std::optional<PedestrianEntranceRow> start = table.findEntrance(from);
    const std::optional<PedestrianEntranceRow> goal = table.findEntrance(to);
    if (!start.has_value() || !goal.has_value()) {
        return {.status = PedestrianRouteStatus::MissingEntrance};
    }
    if (from == to) {
        return {
            .status = PedestrianRouteStatus::Routed,
            .waypointsMeters = {start->positionMeters},
        };
    }

    // build() already proved that every entrance node exists.
    const std::size_t source = *table.findNode(start->node);
    const std::size_t target = *table.findNode(goal->node);
    const SearchResult search = searchFrom(table, source);
    if (search.costs[target] == kUnvisited) {
        return {.status = PedestrianRouteStatus::Unreachable};
    }

    std::vector<std::uint32_t> edgeIndices;
    for (std::size_t node = target; node != source;) {
        const std::uint32_t edge = search.previousEdges[node];
        edgeIndices.push_back(edge);
        node = table.edgeFromNode(edge) == node ? table.edgeToNode(edge)
                                                : table.edgeFromNode(edge);
    }
    std::reverse(edgeIndices.begin(), edgeIndices.end());

    PedestrianRoute route{.status = PedestrianRouteStatus::Routed};
    route.edges.reserve(edgeIndices.size());
    route.waypointsMeters.reserve(edgeIndices.size() + 3);
    appendWaypoint(route.waypointsMeters, start->positionMeters);
    std::size_t node = source;
    appendWaypoint(route.waypointsMeters, table.nodePosition(node));
    for (const std::uint32_t edge : edgeIndices) {
        route.edges.push_back(table.edgeKey(edge));
        node = table.edgeFromNode(edge) == node ? table.edgeToNode(edge)
                                                : table.edgeFromNode(edge);
        appendWaypoint(route.waypointsMeters, table.nodePosition(node));
    }
    appendWaypoint(route.waypointsMeters, goal->positionMeters);
    route.lengthMeters = planarLength(route.waypointsMeters);
    return route;
}

}  // namespace konbini::sim
