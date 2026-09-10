#pragma once
#include "konbini/sim/game_state.h"
#include "konbini/sim/chain_economy_table.h"
#include "konbini/sim/phase1_content.h"
#include "konbini/sim/population_cell_table.h"
namespace konbini::sim {
void resolvePhase1Outcome(GameState& state, const ChainEconomyTable& economy,
                          const PopulationCellTable& cells, const Phase1Content& rules);
}  // namespace konbini::sim
