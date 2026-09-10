#pragma once

#include <cstdint>
#include <array>
#include <optional>

#include "konbini/sim/chain_id.h"
#include "konbini/sim/campaign_state.h"

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
    Result,
    Phase2,
    Phase3,
    BossWarning,
    Boss,
};
inline bool isPlayingPhase(GamePhase phase) noexcept {
    return phase != GamePhase::ChainSelect && phase != GamePhase::Result;
}

enum class MatchOutcome : std::uint8_t { None, Win, Lose, Draw };
enum class MatchEndReason : std::uint8_t { None, Domination, NoCapital, TimeLimit, AllStoresLost, AionSurvived };

// playerChain は ChainSelect 完了までは値を持たない。未選択を既定 chain で
// 補うと、選択前の HUD が他 chain の資金を表示してしまう。
struct GameState {
    std::uint64_t completedTicks = 0;
    GamePhase phase = GamePhase::ChainSelect;
    std::optional<ChainId> playerChain;
    std::uint64_t worldSeed = kFirstPlayableWorldSeed;
    bool competitive = false;
    std::uint64_t phaseTicks = 0;
    MatchOutcome outcome = MatchOutcome::None;
    MatchEndReason endReason = MatchEndReason::None;
    std::array<bool, kFirstPlayableChainCount> hasOpened{};
    std::array<std::uint64_t, kFirstPlayableChainCount> nextAiTick{};
    std::array<std::uint32_t, kSimulationChainCount> destroyedStores{};
    CampaignState campaign;
};

}  // namespace konbini::sim
