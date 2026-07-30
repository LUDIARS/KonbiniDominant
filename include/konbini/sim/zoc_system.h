#pragma once

#include <array>
#include <cstdint>

#include "konbini/sim/chain_economy_table.h"
#include "konbini/sim/population_cell_table.h"
#include "konbini/sim/store_table.h"

// @implements spec/feature/phase-1-dominant-triangle.md ZOC
// @implements spec/feature/economy-and-population.md 顧客化

namespace konbini::sim {

struct ZocSummary {
    std::array<std::uint64_t, kFirstPlayableChainCount> capturedPopulation{};
    std::uint32_t unassignedCellCount = 0;
};

[[nodiscard]] ZocSummary assignNearestStores(
    StoreTable& stores, PopulationCellTable& populationCells,
    ChainEconomyTable& economy);

}  // namespace konbini::sim
