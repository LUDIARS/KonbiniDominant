#pragma once
#include <optional>
#include "konbini/sim/placement_system.h"
#include "konbini/sim/dominant_triangle.h"
#include "konbini/sim/population_cell_table.h"
namespace konbini::sim {
[[nodiscard]] std::uint32_t opponentPeriodTicks(std::uint64_t phaseTicks, const Phase1Content& rules);
[[nodiscard]] std::optional<PlaceStoreCommand> chooseOpponentPlacement(
    ChainId chain, const GameState& state, const FacilityTable& facilities,
    const StoreTable& stores, const PopulationCellTable& cells,
    const ChainEconomyTable& economy, const Phase1Content& rules);
}  // namespace konbini::sim
