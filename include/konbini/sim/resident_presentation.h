#pragma once

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "konbini/sim/first_playable_content.h"
#include "konbini/sim/population_cell_table.h"
#include "konbini/sim/store_table.h"

// @implements spec/feature/npc-conversations-and-placement-feedback.md Ambient resident baseline
// @implements spec/interface/visia-presentation.md Pictor integration boundary
// @implements spec/interface/pictor-rendering.md `RenderSnapshot`

namespace konbini::sim {

enum class ResidentTripPhase : std::uint8_t {
    AtHome = 0,
    WalkingToStore,
    AtStore,
    WalkingHome,
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
    std::optional<std::string> speech;
    ResidentBubbleConfig bubble{};
};

// Pure projection: no resident state or random-consumption cursor is retained.
// The same authoritative tables, content, world seed, and completed tick yield
// the same presentation records.
[[nodiscard]] std::vector<ResidentPresentation>
projectResidentPresentations(
    std::uint64_t completedTicks, std::uint64_t worldSeed,
    std::uint32_t ticksPerSecond,
    const ResidentPresentationContent& content,
    const PopulationCellTable& populationCells, const StoreTable& stores);

}  // namespace konbini::sim
