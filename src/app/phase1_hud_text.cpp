#include "konbini/app/phase1_hud_text.h"
#include <algorithm>
#include <stdexcept>
namespace konbini::app {
namespace {
std::string clockText(const std::uint64_t ticks, const std::uint32_t rate) {
    if (rate == 0) { throw std::invalid_argument("HUD tick rate is zero"); }
    const auto seconds = (ticks + rate - 1) / rate;
    const auto remainder = seconds % 60;
    return std::to_string(seconds / 60) + ":" + (remainder < 10 ? "0" : "") + std::to_string(remainder);
}
std::string chainStats(const sim::ChainMatchView& chain) {
    return hudChainLabel(chain.chain) + " S" + std::to_string(chain.stores) +
        " C" + std::to_string(chain.customers) + " T" + std::to_string(chain.triangles);
}
std::string resultTitle(const sim::MatchOutcome outcome) {
    switch (outcome) {
        case sim::MatchOutcome::Win: return "VICTORY";
        case sim::MatchOutcome::Lose: return "DEFEAT";
        case sim::MatchOutcome::Draw: return "DRAW";
        case sim::MatchOutcome::None: break;
    }
    throw std::logic_error("result screen has no match outcome");
}
std::string resultReason(const sim::MatchEndReason reason) {
    switch (reason) {
        case sim::MatchEndReason::Domination: return "CITY DOMINATED";
        case sim::MatchEndReason::NoCapital: return "NO STORES - NO REBUILD FUNDS";
        case sim::MatchEndReason::TimeLimit: return "TIME UP - MOST CUSTOMERS WINS";
        case sim::MatchEndReason::None: break;
    }
    throw std::logic_error("result screen has no match reason");
}
}
// @implements spec/feature/phase-1-game-loop.md Match contract
std::vector<std::string> buildPhase1HudLines(const HudTextInput& input) {
    const auto& hud = input.hud;
    std::vector<std::string> lines{"KONBINI DOMINANT"};
    if (hud.phase == sim::GamePhase::ChainSelect) {
        lines.emplace_back("PHASE 1 - FIVE MINUTE CITY CONQUEST");
        lines.emplace_back("CHOOSE YOUR CHAIN");
        for (std::size_t i = 0; i < hud.chains.size(); ++i) {
            const auto& chain = hud.chains[i];
            lines.push_back(std::to_string(i + 1) + " " + hudChainLabel(chain.chain) +
                            " COST " + std::to_string(chain.buildCost));
        }
        lines.emplace_back("START WITH FUNDS FOR FIVE STORES");
        lines.emplace_back("CLICK A LOT TWICE TO BUILD");
        lines.emplace_back("THREE STORES FORM A TRIANGLE");
        lines.emplace_back("ENCIRCLE RIVALS FOR THREE SECONDS");
        lines.emplace_back("DESTROY ALL RIVALS AND CONTROL");
        lines.push_back(std::to_string(hud.dominationPercent) + " PERCENT OF THE CITY TO WIN");
        lines.emplace_back("TIME UP - MOST CUSTOMERS WINS");
        lines.emplace_back("WASD MOVE / WHEEL ZOOM");
        lines.emplace_back("P PAUSE / F1 HELP");
        return lines;
    }
    if (hud.phase == sim::GamePhase::Result) {
        lines.push_back(resultTitle(hud.outcome));
        lines.push_back(resultReason(hud.endReason));
        lines.push_back("TIME " + clockText(hud.phaseTicks, hud.ticksPerSecond));
        lines.emplace_back("S STORES / C CUSTOMERS / T TRIANGLES");
        for (const auto& chain : hud.chains) { lines.push_back(chainStats(chain)); }
        lines.push_back("RIVALS DESTROYED " + std::to_string(hud.destroyedRivalStores));
        lines.emplace_back("R - PLAY AGAIN");
        return lines;
    }
    if (!hud.playerChain) { throw std::logic_error("active match has no player chain"); }
    const auto& player = hud.chains[sim::chainIndex(*hud.playerChain)];
    lines.push_back(std::string(input.isPaused ? "PAUSED" : "PHASE 1") +
                    " - " + clockText(hud.ticksRemaining, hud.ticksPerSecond));
    lines.push_back(hudChainLabel(*hud.playerChain) + " CASH " + std::to_string(hud.cashCredits));
    lines.push_back("INCOME " + std::to_string(hud.predictedEconomyIncomeCredits) +
                    " IN " + clockText(hud.ticksUntilEconomy, hud.ticksPerSecond));
    lines.push_back("CUSTOMERS " + std::to_string(player.customers) + "/" + std::to_string(hud.totalPopulation));
    lines.push_back("TRIANGLES " + std::to_string(player.triangles) +
                    " DESTROYED " + std::to_string(hud.destroyedRivalStores));
    lines.push_back("ENEMY BUILD INTERVAL " + std::to_string(hud.aiPeriodTicks / hud.ticksPerSecond) +
                    "." + std::to_string((hud.aiPeriodTicks % hud.ticksPerSecond) * 10 / hud.ticksPerSecond) + "S");
    for (const auto& chain : hud.chains) {
        if (chain.chain != *hud.playerChain) { lines.push_back(chainStats(chain)); }
    }
    if (hud.threatenedPlayerStores != 0) {
        lines.push_back("DANGER - " + std::to_string(hud.threatenedPlayerStores) + " STORES ENCIRCLED");
    }
    if (input.selectedFacility) {
        lines.push_back(input.selectionIsPlacementCandidate ?
            "BUILD COST " + std::to_string(input.selectedBuildCostCredits) + " - CLICK AGAIN" :
            "SELECTED LOT IS OCCUPIED");
    } else {
        lines.emplace_back("CLICK A LOT TWICE TO BUILD");
    }
    if (input.lastPlacementFailure && *input.lastPlacementFailure != sim::PlacementFailure::None) {
        lines.push_back(std::string(hudPlacementFailureText(*input.lastPlacementFailure)));
    }
    if (input.showControls) {
        lines.push_back("WIN - NO RIVALS AND " + std::to_string(hud.dominationPercent) + " PERCENT CUSTOMERS");
        lines.emplace_back("3 STORES - TRIANGLE - MORE INCOME");
        lines.emplace_back("3 SECONDS ENCIRCLED - STORE LOST");
        lines.emplace_back("WASD MOVE / WHEEL ZOOM / ESC CANCEL");
        lines.emplace_back("P PAUSE / F1 HIDE HELP");
    }
    if (input.droppedTicks != 0) {
        lines.push_back("DROPPED TICKS " + std::to_string(input.droppedTicks));
    }
    return lines;
}
}  // namespace konbini::app
