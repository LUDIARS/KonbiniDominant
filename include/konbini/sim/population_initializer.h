#pragma once

#include <cstdint>

#include "konbini/sim/counter_rng.h"
#include "konbini/sim/facility_table.h"
#include "konbini/sim/first_playable_content.h"
#include "konbini/sim/population_cell_table.h"

// @implements spec/feature/economy-and-population.md 人口
// @implements spec/design.md 5. Tick と決定性

namespace konbini::sim {

void initializePopulationCells(
    const FacilityTable& facilities, const FirstPlayableContent& content,
    std::uint64_t worldSeed, PopulationCellTable& populationCells,
    GenerationalIdPool<PopulationCellId>& populationCellIds);

}  // namespace konbini::sim
