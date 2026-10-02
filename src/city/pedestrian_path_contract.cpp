#include "konbini/city/pedestrian_path_contract.h"

#include <algorithm>
#include <stdexcept>

// @implements spec/interface/figmentum-city-generation.md Pedestrian path contract

namespace konbini::city {

namespace {

[[nodiscard]] std::vector<sim::PedestrianNodeRow> nodeRows(
    const PedestrianPathContract& contract) {
    std::vector<sim::PedestrianNodeRow> rows;
    rows.reserve(contract.nodes.size());
    for (const PedestrianPathNode& node : contract.nodes) {
        rows.push_back({.key = node.key, .positionMeters = node.positionMeters});
    }
    return rows;
}

[[nodiscard]] std::vector<sim::PedestrianEdgeRow> edgeRows(
    const PedestrianPathContract& contract) {
    std::vector<sim::PedestrianEdgeRow> rows;
    rows.reserve(contract.edges.size());
    for (const PedestrianPathEdge& edge : contract.edges) {
        rows.push_back({.key = edge.key,
                        .from = edge.fromNode,
                        .to = edge.toNode,
                        .lengthMeters = edge.lengthMeters});
    }
    return rows;
}

[[nodiscard]] std::vector<sim::PedestrianEntranceRow> entranceRows(
    const PedestrianPathContract& contract) {
    std::vector<sim::PedestrianEntranceRow> rows;
    rows.reserve(contract.entrances.size());
    for (const PedestrianPathEntrance& entrance : contract.entrances) {
        rows.push_back({.facility = entrance.facility,
                        .positionMeters = entrance.positionMeters,
                        .node = entrance.node});
    }
    return rows;
}

}  // namespace

// @implements spec/interface/figmentum-city-generation.md Pedestrian path contract
void validatePedestrianPathContract(
    const PedestrianPathContract& contract,
    const std::span<const sim::FigmentumFacilityKey> facilityKeys) {
    if (contract.schemaVersion != kSupportedPedestrianNetworkSchemaVersion ||
        contract.recipeVersion != kSupportedPedestrianNetworkRecipeVersion) {
        throw std::invalid_argument(
            "unsupported Figmentum pedestrian network version");
    }
    if (contract.nodes.empty()) {
        throw std::invalid_argument("pedestrian path contract has no nodes");
    }
    for (const PedestrianPathEntrance& entrance : contract.entrances) {
        if (entrance.side > PedestrianEntranceSide::West) {
            throw std::invalid_argument("pedestrian entrance side is invalid");
        }
    }
    // Every manifest facility walks from / to its own entrance. A missing or
    // foreign entrance means the contract was derived from a different plan.
    if (contract.entrances.size() != facilityKeys.size() ||
        !std::equal(contract.entrances.begin(),
                    contract.entrances.end(),
                    facilityKeys.begin(),
                    [](const PedestrianPathEntrance& entrance,
                       const sim::FigmentumFacilityKey key) {
                        return entrance.facility == key;
                    })) {
        throw std::invalid_argument(
            "pedestrian entrances must match the manifest facilities one-to-one");
    }
    // Ordering, references, finiteness and edge length share the table's
    // validation so the contract and the runtime table cannot disagree.
    static_cast<void>(toPedestrianPathTable(contract));
}

// @implements spec/interface/figmentum-city-generation.md Pedestrian path contract
sim::PedestrianPathTable toPedestrianPathTable(
    const PedestrianPathContract& contract) {
    const std::vector<sim::PedestrianNodeRow> nodes = nodeRows(contract);
    const std::vector<sim::PedestrianEdgeRow> edges = edgeRows(contract);
    const std::vector<sim::PedestrianEntranceRow> entrances =
        entranceRows(contract);
    return sim::PedestrianPathTable::build(nodes, edges, entrances);
}

}  // namespace konbini::city
