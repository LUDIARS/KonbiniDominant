#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "konbini/sim/chain_economy_table.h"
#include "konbini/sim/facility_table.h"
#include "konbini/sim/first_playable_content.h"
#include "konbini/sim/game_state.h"
#include "konbini/sim/population_cell_table.h"
#include "konbini/sim/store_table.h"

// @implements spec/plan/tasks/first-playable.md Simulation

namespace konbini::sim {

inline constexpr std::uint32_t kCanonicalSnapshotSchemaVersion = 1;

struct CanonicalSnapshot {
    std::uint32_t schemaVersion = kCanonicalSnapshotSchemaVersion;
    std::vector<std::byte> bytes;
    std::uint64_t hash = 0;
};

[[nodiscard]] CanonicalSnapshot makeCanonicalSnapshot(
    const GameState& state, const FirstPlayableContent& content,
    const FacilityTable& facilities, const StoreTable& stores,
    const PopulationCellTable& populationCells,
    const ChainEconomyTable& economy);

}  // namespace konbini::sim
