#pragma once

#include <cstdint>

#include "konbini/sim/chain_economy_table.h"
#include "konbini/sim/game_state.h"
#include "konbini/sim/player_command.h"

// @implements spec/feature/chain-selection.md 共通
// @implements spec/feature/game-flow.md 状態機械

namespace konbini::sim {

enum class SelectChainFailure : std::uint8_t {
    None = 0,
    WrongPhase,
    ChainAlreadySelected,
    ChainAlreadyActive,
    InvalidChain,
};

[[nodiscard]] SelectChainFailure applySelectChain(
    const SelectChainCommand& command, GameState& state,
    ChainEconomyTable& economy);

}  // namespace konbini::sim
