#include "konbini/sim/pedestrian_path_table.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

// @implements spec/feature/npc-conversations-and-placement-feedback.md Pedestrian path walking

namespace konbini::sim {

namespace {

constexpr std::size_t kMaximumIndex = std::numeric_limits<std::uint32_t>::max();
// Longest edge whose millimetre cost still sums safely over a full route.
constexpr double kMaximumEdgeLengthMeters = 1.0e9;

[[nodiscard]] std::uint32_t requireNode(
    const std::vector<PedestrianNodeKey>& keys, const PedestrianNodeKey key,
    const char* const message) {
    const auto found = std::lower_bound(keys.begin(), keys.end(), key);
    if (found == keys.end() || *found != key) {
        throw std::invalid_argument(message);
    }
    return static_cast<std::uint32_t>(found - keys.begin());
}

[[nodiscard]] std::uint64_t costMillimeters(const double lengthMeters) {
    if (!std::isfinite(lengthMeters) || lengthMeters <= 0.0 ||
        lengthMeters > kMaximumEdgeLengthMeters) {
        throw std::invalid_argument(
            "pedestrian edge length must be finite and positive");
    }
    const auto cost =
        static_cast<std::uint64_t>(std::llround(lengthMeters * 1000.0));
    // A sub-millimetre edge would cost zero and make Dijkstra ambiguous.
    if (cost == 0) {
        throw std::invalid_argument("pedestrian edge is shorter than 1 mm");
    }
    return cost;
}

}  // namespace

// @implements spec/feature/npc-conversations-and-placement-feedback.md Pedestrian path walking
PedestrianPathTable PedestrianPathTable::build(
    const std::span<const PedestrianNodeRow> nodes,
    const std::span<const PedestrianEdgeRow> edges,
    const std::span<const PedestrianEntranceRow> entrances) {
    if (nodes.size() > kMaximumIndex || edges.size() > kMaximumIndex ||
        entrances.size() > kMaximumIndex) {
        throw std::invalid_argument("pedestrian path table is too large");
    }

    PedestrianPathTable table;
    table.nodeKeys_.reserve(nodes.size());
    table.nodePositions_.reserve(nodes.size());
    for (const PedestrianNodeRow& node : nodes) {
        if (node.key == 0 || !isFinite(node.positionMeters)) {
            throw std::invalid_argument(
                "pedestrian node key or position is invalid");
        }
        if (!table.nodeKeys_.empty() && node.key <= table.nodeKeys_.back()) {
            throw std::invalid_argument(
                "pedestrian nodes must be in strictly ascending key order");
        }
        table.nodeKeys_.push_back(node.key);
        table.nodePositions_.push_back(node.positionMeters);
    }

    std::vector<std::uint32_t> degree(nodes.size(), 0);
    table.edgeKeys_.reserve(edges.size());
    table.edgeFrom_.reserve(edges.size());
    table.edgeTo_.reserve(edges.size());
    table.edgeCostsMillimeters_.reserve(edges.size());
    for (const PedestrianEdgeRow& edge : edges) {
        if (edge.key == 0) {
            throw std::invalid_argument("pedestrian edge key is invalid");
        }
        if (!table.edgeKeys_.empty() && edge.key <= table.edgeKeys_.back()) {
            throw std::invalid_argument(
                "pedestrian edges must be in strictly ascending key order");
        }
        if (edge.from >= edge.to) {
            throw std::invalid_argument(
                "pedestrian edge endpoints must satisfy from < to");
        }
        const std::uint32_t from = requireNode(
            table.nodeKeys_, edge.from, "pedestrian edge references an unknown node");
        const std::uint32_t to = requireNode(
            table.nodeKeys_, edge.to, "pedestrian edge references an unknown node");
        table.edgeKeys_.push_back(edge.key);
        table.edgeFrom_.push_back(from);
        table.edgeTo_.push_back(to);
        table.edgeCostsMillimeters_.push_back(costMillimeters(edge.lengthMeters));
        ++degree[from];
        ++degree[to];
    }

    // CSR adjacency. Edges are visited in key order, so each node's slice is
    // already sorted by edge key.
    table.adjacencyOffsets_.assign(nodes.size() + 1, 0);
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        table.adjacencyOffsets_[index + 1] =
            table.adjacencyOffsets_[index] + degree[index];
    }
    table.adjacencyEdges_.assign(table.adjacencyOffsets_.back(), 0);
    std::vector<std::uint32_t> cursor(table.adjacencyOffsets_.begin(),
                                      table.adjacencyOffsets_.end() - 1);
    for (std::uint32_t edge = 0; edge < table.edgeKeys_.size(); ++edge) {
        table.adjacencyEdges_[cursor[table.edgeFrom_[edge]]++] = edge;
        table.adjacencyEdges_[cursor[table.edgeTo_[edge]]++] = edge;
    }

    table.entranceFacilities_.reserve(entrances.size());
    table.entrancePositions_.reserve(entrances.size());
    table.entranceNodes_.reserve(entrances.size());
    for (const PedestrianEntranceRow& entrance : entrances) {
        if (!entrance.facility.isValid() || !isFinite(entrance.positionMeters)) {
            throw std::invalid_argument(
                "pedestrian entrance facility or position is invalid");
        }
        if (!table.entranceFacilities_.empty() &&
            entrance.facility <= table.entranceFacilities_.back()) {
            throw std::invalid_argument(
                "pedestrian entrances must be in strictly ascending facility order");
        }
        table.entranceFacilities_.push_back(entrance.facility);
        table.entrancePositions_.push_back(entrance.positionMeters);
        table.entranceNodes_.push_back(requireNode(
            table.nodeKeys_,
            entrance.node,
            "pedestrian entrance references an unknown node"));
    }
    return table;
}

std::size_t PedestrianPathTable::nodeCount() const noexcept {
    return nodeKeys_.size();
}

std::size_t PedestrianPathTable::edgeCount() const noexcept {
    return edgeKeys_.size();
}

std::size_t PedestrianPathTable::entranceCount() const noexcept {
    return entranceFacilities_.size();
}

PedestrianNodeKey PedestrianPathTable::nodeKey(const std::size_t nodeIndex) const {
    return nodeKeys_.at(nodeIndex);
}

Vec3 PedestrianPathTable::nodePosition(const std::size_t nodeIndex) const {
    return nodePositions_.at(nodeIndex);
}

std::optional<std::size_t> PedestrianPathTable::findNode(
    const PedestrianNodeKey key) const noexcept {
    const auto found = std::lower_bound(nodeKeys_.begin(), nodeKeys_.end(), key);
    if (found == nodeKeys_.end() || *found != key) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(found - nodeKeys_.begin());
}

PedestrianEdgeKey PedestrianPathTable::edgeKey(const std::size_t edgeIndex) const {
    return edgeKeys_.at(edgeIndex);
}

std::size_t PedestrianPathTable::edgeFromNode(const std::size_t edgeIndex) const {
    return edgeFrom_.at(edgeIndex);
}

std::size_t PedestrianPathTable::edgeToNode(const std::size_t edgeIndex) const {
    return edgeTo_.at(edgeIndex);
}

std::uint64_t PedestrianPathTable::edgeCostMillimeters(
    const std::size_t edgeIndex) const {
    return edgeCostsMillimeters_.at(edgeIndex);
}

std::span<const std::uint32_t> PedestrianPathTable::adjacentEdges(
    const std::size_t nodeIndex) const {
    const std::uint32_t begin = adjacencyOffsets_.at(nodeIndex);
    const std::uint32_t end = adjacencyOffsets_.at(nodeIndex + 1);
    return std::span<const std::uint32_t>(adjacencyEdges_).subspan(begin, end - begin);
}

// @implements spec/feature/npc-conversations-and-placement-feedback.md Pedestrian path walking
std::optional<PedestrianEntranceRow> PedestrianPathTable::findEntrance(
    const FigmentumFacilityKey facility) const noexcept {
    const auto found = std::lower_bound(
        entranceFacilities_.begin(), entranceFacilities_.end(), facility);
    if (found == entranceFacilities_.end() || *found != facility) {
        return std::nullopt;
    }
    const auto index = static_cast<std::size_t>(found - entranceFacilities_.begin());
    return PedestrianEntranceRow{
        .facility = facility,
        .positionMeters = entrancePositions_[index],
        .node = nodeKeys_[entranceNodes_[index]],
    };
}

}  // namespace konbini::sim
