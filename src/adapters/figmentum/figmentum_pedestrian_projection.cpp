#include "figmentum_pedestrian_projection.h"

#include <stdexcept>
#include <string>

#include "figmentum/gen/pedestrian_path_error.h"

#include "figmentum_conversions.h"

// @implements spec/interface/figmentum-city-generation.md Pedestrian path contract

namespace konbini::adapters::figmentum {

namespace {

[[nodiscard]] const char* codeName(const fg::PedestrianPathErrorCode code) noexcept {
    switch (code) {
        case fg::PedestrianPathErrorCode::InvalidParams:
            return "InvalidParams";
        case fg::PedestrianPathErrorCode::UnsupportedVersion:
            return "UnsupportedVersion";
        case fg::PedestrianPathErrorCode::PlanMismatch:
            return "PlanMismatch";
        case fg::PedestrianPathErrorCode::InvalidKey:
            return "InvalidKey";
        case fg::PedestrianPathErrorCode::NonCanonicalOrder:
            return "NonCanonicalOrder";
        case fg::PedestrianPathErrorCode::NonFiniteCoordinate:
            return "NonFiniteCoordinate";
        case fg::PedestrianPathErrorCode::DegenerateEdge:
            return "DegenerateEdge";
        case fg::PedestrianPathErrorCode::UnknownNode:
            return "UnknownNode";
        case fg::PedestrianPathErrorCode::Unreachable:
            return "Unreachable";
    }
    return "Unknown";
}

// @implements spec/interface/figmentum-city-generation.md Pedestrian path contract
city::PedestrianEntranceSide convert(const fg::PedestrianSide side) {
    switch (side) {
        case fg::PedestrianSide::South:
            return city::PedestrianEntranceSide::South;
        case fg::PedestrianSide::East:
            return city::PedestrianEntranceSide::East;
        case fg::PedestrianSide::North:
            return city::PedestrianEntranceSide::North;
        case fg::PedestrianSide::West:
            return city::PedestrianEntranceSide::West;
    }
    throw std::runtime_error("Figmentum returned an unknown pedestrian entrance side");
}

}  // namespace

// @implements spec/interface/figmentum-city-generation.md Pedestrian path contract
city::PedestrianPathContract convertPedestrianNetwork(
    const fg::PedestrianNetwork& network, const fg::CityPlan& plan) {
    if (network.schemaVersion != fg::kPedestrianNetworkSchemaVersion ||
        network.schemaVersion != city::kSupportedPedestrianNetworkSchemaVersion ||
        network.recipeVersion != fg::kPedestrianNetworkRecipeVersion ||
        network.recipeVersion != city::kSupportedPedestrianNetworkRecipeVersion) {
        throw std::runtime_error(
            "Figmentum returned an unsupported pedestrian network version");
    }
    if (network.citySchemaVersion != plan.schemaVersion ||
        network.cityRecipeVersion != plan.recipeVersion ||
        network.seed != plan.seed) {
        throw std::runtime_error(
            "Figmentum pedestrian network was derived from a different city plan");
    }

    city::PedestrianPathContract contract{
        .schemaVersion = network.schemaVersion,
        .recipeVersion = network.recipeVersion,
    };
    contract.nodes.reserve(network.nodes.size());
    for (const fg::PedestrianNode& node : network.nodes) {
        contract.nodes.push_back(
            {.key = node.key, .positionMeters = toVec3(node.position)});
    }
    contract.edges.reserve(network.edges.size());
    for (const fg::PedestrianEdge& edge : network.edges) {
        contract.edges.push_back({
            .key = edge.key,
            .fromNode = edge.from,
            .toNode = edge.to,
            .lengthMeters = static_cast<double>(edge.length),
        });
    }
    contract.entrances.reserve(network.entrances.size());
    for (const fg::PedestrianEntrance& entrance : network.entrances) {
        contract.entrances.push_back({
            .facility = sim::FigmentumFacilityKey{.rawValue = entrance.facility},
            .side = convert(entrance.side),
            .positionMeters = toVec3(entrance.position),
            .node = entrance.node,
        });
    }
    return contract;
}

// @implements spec/interface/figmentum-city-generation.md Pedestrian path contract
city::PedestrianPathContract projectPedestrianNetwork(
    const fg::CityPlanParams& params, const fg::CityPlan& plan) {
    try {
        return convertPedestrianNetwork(fg::planPedestrianNetwork(params, plan), plan);
    } catch (const fg::PedestrianPathError& error) {
        throw std::runtime_error(std::string("Figmentum pedestrian network rejected (") +
                                 codeName(error.code()) + "): " + error.what());
    }
}

}  // namespace konbini::adapters::figmentum
