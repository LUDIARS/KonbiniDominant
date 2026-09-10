#pragma once
#include <span>
#include <vector>
#include "konbini/sim/encirclement.h"
#include "konbini/sim/game_state.h"
#include "konbini/sim/facility_table.h"
#include "konbini/sim/chain_economy_table.h"
#include "konbini/sim/population_cell_table.h"
namespace konbini::sim {
// Called on the tick's staged tables; the outer transaction commits all changes.
[[nodiscard]] std::vector<Encirclement> resolveEncirclement(
    GameState& state, StoreTable& stores, FacilityTable& facilities,
    ChainEconomyTable& economy, PopulationCellTable& population,
    std::span<const DominantTriangle> triangles,
    std::span<const Encirclement> previous, const Phase1Content& rules);
}  // namespace konbini::sim
