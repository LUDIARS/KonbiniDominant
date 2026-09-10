#pragma once
#include "konbini/sim/population_cell_table.h"
#include "konbini/sim/store_table.h"
#include "konbini/sim/phase1_content.h"
namespace konbini::sim {
void applyPopulationLoss(PopulationCellTable& cells, FacilityId facility, std::uint32_t percent);
void recoverServedPopulation(PopulationCellTable& cells, const StoreTable& stores,
                            const Phase1Content& rules);
}  // namespace konbini::sim
