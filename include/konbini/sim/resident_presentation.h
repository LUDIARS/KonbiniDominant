#pragma once

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "konbini/sim/facility_table.h"
#include "konbini/sim/first_playable_content.h"
#include "konbini/sim/pedestrian_path_table.h"
#include "konbini/sim/population_cell_table.h"
#include "konbini/sim/store_table.h"

// @implements spec/feature/npc-conversations-and-placement-feedback.md Ambient resident baseline
// @implements spec/interface/visia-presentation.md Pictor integration boundary
// @implements spec/interface/pictor-rendering.md `RenderSnapshot`
// @implements spec/feature/npc-conversations-and-placement-feedback.md Pedestrian path walking

namespace konbini::sim {

enum class ResidentTripPhase : std::uint8_t {
    AtHome = 0,
    WalkingToStore,
    AtStore,
    WalkingHome,
};

// How the sample's trip is routed. The two failure states are explicit so a
// city with a pedestrian network never degrades to a straight line silently.
enum class ResidentRouteState : std::uint8_t {
    // Unassigned cell: the sample stays home and has no trip.
    NoTrip = 0,
    // No pedestrian network was supplied (grid town): BASE-NPC-PATH-01.
    DirectLine,
    // Walking the Figmentum pedestrian path from entrance to entrance.
    PedestrianPath,
    // The home or store facility has no pedestrian entrance.
    MissingEntrance,
    // The entrances are not connected in the pedestrian graph.
    Unreachable,
};

// A resident sample is derived from a population cell and ordinal. It is not
// an authoritative EntityId and is never persisted or included in canonical
// state.
struct ResidentPresentationId {
    PopulationCellId populationCellId{};
    std::uint32_t ordinal = 0;

    auto operator<=>(const ResidentPresentationId&) const = default;
};

struct ResidentBubbleConfig {
    double heightMeters = 0.0;
    double maxDistanceMeters = 0.0;
};

struct ResidentPresentation {
    ResidentPresentationId id{};
    ResidentTripPhase phase = ResidentTripPhase::AtHome;
    Vec3 positionMeters{};
    // Radians around +Y. Zero faces +Z and positive values turn toward +X.
    double yawRadians = 0.0;
    std::optional<StoreId> targetStore;
    ResidentRouteState route = ResidentRouteState::NoTrip;
    std::optional<std::string> speech;
    ResidentBubbleConfig bubble{};
};

// Read-only pedestrian graph plus the facility table that maps game facility
// ids to Figmentum keys. Both are borrowed for the duration of one projection.
struct ResidentPathContext {
    const FacilityTable& facilities;
    const PedestrianPathTable& paths;
};

// Pure projection: no resident state or random-consumption cursor is retained.
// The same authoritative tables, content, world seed, and completed tick yield
// the same presentation records. With `paths`, trips follow the pedestrian
// network; without it they keep the BASE-NPC-PATH-01 straight line.
[[nodiscard]] std::vector<ResidentPresentation>
projectResidentPresentations(
    std::uint64_t completedTicks, std::uint64_t worldSeed,
    std::uint32_t ticksPerSecond,
    const ResidentPresentationContent& content,
    const PopulationCellTable& populationCells, const StoreTable& stores,
    const ResidentPathContext* paths = nullptr);

}  // namespace konbini::sim
