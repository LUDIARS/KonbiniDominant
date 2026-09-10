#pragma once
#include <span>
#include "konbini/sim/dominant_triangle.h"
#include "konbini/sim/population_cell_table.h"
#include "konbini/sim/chain_economy_table.h"
#include "konbini/sim/skill_state.h"
namespace konbini::sim {
void assignCompetitiveCustomers(StoreTable& stores, PopulationCellTable& cells,
                                ChainEconomyTable& economy,
                                std::span<const DominantTriangle> triangles,
                                const Phase1Content& rules, bool useFaith = false,
                                const SkillInfluence& skills = {});
}  // namespace konbini::sim
