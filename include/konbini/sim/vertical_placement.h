#pragma once
#include "konbini/sim/game_state.h"
#include "konbini/sim/campaign_content.h"
namespace konbini::sim {
inline bool verticalUnlocked(const GameState& state) noexcept {
    return state.campaign.enabled && isPlayingPhase(state.phase) && state.phase!=GamePhase::Phase1;
}
inline std::uint64_t matchClock(const GameState& state) noexcept {
    return state.campaign.enabled ? state.campaign.elapsedTicks : state.phaseTicks;
}
std::int64_t verticalBuildCost(std::int64_t base,std::uint32_t slot,const VerticalContent& rules);
}
