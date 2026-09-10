#include "konbini/sim/phase1_outcome_system.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace konbini::sim {
namespace {
void finish(GameState& state, const MatchOutcome outcome, const MatchEndReason reason) noexcept {
    state.phase = GamePhase::Result;
    state.outcome = outcome;
    state.endReason = reason;
}
}
// @implements spec/feature/phase-1-game-loop.md Match contract
void resolvePhase1Outcome(GameState& state, const ChainEconomyTable& economy,
                          const PopulationCellTable& cells, const Phase1Content& rules) {
    if (state.phase != GamePhase::Phase1 || !state.playerChain) { return; }
    const auto player = economy.row(*state.playerChain);
    if (player.storeCount == 0 &&
        !economy.canAfford(*state.playerChain, economy.rules(*state.playerChain).buildCostCredits)) {
        finish(state, MatchOutcome::Lose, MatchEndReason::NoCapital);
        return;
    }
    bool rivalsGone = true;
    for (std::size_t i = 0; i < kFirstPlayableChainCount; ++i) {
        if (static_cast<ChainId>(i) == *state.playerChain) { continue; }
        rivalsGone = rivalsGone && state.hasOpened[i] &&
            economy.row(static_cast<ChainId>(i)).storeCount == 0;
    }
    std::uint64_t totalPopulation = 0;
    for (std::size_t i = 0; i < cells.size(); ++i) {
        const auto population = cells.row(i).population;
        if (totalPopulation > std::numeric_limits<std::uint64_t>::max() - population) {
            throw std::overflow_error("total population overflow");
        }
        totalPopulation += population;
    }
    // ceil(total * percent / 100) without overflowing the intermediate product.
    const auto target = (totalPopulation / 100) * rules.dominationPercent +
        ((totalPopulation % 100) * rules.dominationPercent + 99) / 100;
    if (rivalsGone && totalPopulation != 0 && player.customerShare >= target) {
        finish(state, MatchOutcome::Win, MatchEndReason::Domination);
        return;
    }
    if (state.phaseTicks < rules.durationTicks) { return; }
    std::uint64_t best = player.customerShare;
    unsigned leaders = 0;
    for (std::size_t i = 0; i < kFirstPlayableChainCount; ++i) {
        best = std::max(best, economy.row(static_cast<ChainId>(i)).customerShare);
    }
    for (std::size_t i = 0; i < kFirstPlayableChainCount; ++i) {
        if (economy.row(static_cast<ChainId>(i)).customerShare == best) { ++leaders; }
    }
    finish(state, player.customerShare < best ? MatchOutcome::Lose :
                  leaders > 1 ? MatchOutcome::Draw : MatchOutcome::Win,
           MatchEndReason::TimeLimit);
}
}  // namespace konbini::sim
