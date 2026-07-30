#pragma once

#include <cstdint>
#include <optional>

#include "konbini/sim/chain_id.h"

// @implements spec/data/world-state.md `GameState`
// @implements spec/feature/game-flow.md 状態機械

namespace konbini::sim {

// first playable は固定 seed で 1 都市だけを生成する。city 側の manifest
// validation も同じ値を要求するので、定数はここを唯一の正本にする。
inline constexpr std::uint64_t kFirstPlayableWorldSeed = 42;

// spec/feature/game-flow.md の状態機械のうち first playable で到達する範囲。
// Phase 2 以降を足すときも既存値を renumber しない (save の key になる)。
enum class GamePhase : std::uint8_t {
    ChainSelect = 0,
    Phase1,
};

// playerChain は ChainSelect 完了までは値を持たない。未選択を既定 chain で
// 補うと、選択前の HUD が他 chain の資金を表示してしまう。
struct GameState {
    std::uint64_t completedTicks = 0;
    GamePhase phase = GamePhase::ChainSelect;
    std::optional<ChainId> playerChain;
    std::uint64_t worldSeed = kFirstPlayableWorldSeed;
};

}  // namespace konbini::sim
