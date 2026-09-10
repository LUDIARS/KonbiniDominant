#include "konbini/sim/opponent_ai_system.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include "konbini/sim/vertical_placement.h"
namespace konbini::sim {
namespace {
double distanceSquared(const Vec3 a, const Vec3 b) noexcept {
    return (a.x-b.x)*(a.x-b.x) + (a.z-b.z)*(a.z-b.z);
}
double candidateScore(const FacilityRow& facility, const ChainId chain,
                      const StoreTable& stores, const PopulationCellTable& cells,
                      const ChainEconomyTable& economy, const Phase1Content& rules,
                      const std::vector<DominantTriangle>& enemyTriangles) {
    double score = 0.0;
    const double radius = economy.rules(chain).zocRadiusMeters;
    for (std::size_t i = 0; i < cells.size(); ++i) {
        const auto cell = cells.row(i);
        if (cell.dimension != facility.dimension ||
            distanceSquared(cell.positionMeters, facility.positionMeters) > radius * radius) { continue; }
        const double weight = !cell.preferredChain ? 3.0 : *cell.preferredChain == chain ? 1.0 : 4.0;
        score += static_cast<double>(cell.population) * weight;
    }
    std::vector<StoreRow> neighbors;
    const double maxEdge = rules.triangleMaxEdgeMeters * rules.triangleMaxEdgeMeters;
    for (std::size_t i = 0; i < stores.size(); ++i) {
        const auto store = stores.row(i);
        if (store.isActive && store.chain == chain && store.dimension == facility.dimension &&
            distanceSquared(store.positionMeters, facility.positionMeters) <= maxEdge) {
            if (std::ranges::none_of(neighbors, [&](const auto& prior) { return prior.facilityId == store.facilityId; })) {
                neighbors.push_back(store);
            }
        }
    }
    // A triangle is rewarded once per candidate; additional pairs must not
    // overpower actual demand merely because an area already has many stores.
    double triangleScore = 0.0;
    for (std::size_t a = 0; a < neighbors.size(); ++a) {
        for (std::size_t b = a + 1; b < neighbors.size(); ++b) {
            const auto pa = neighbors[a].positionMeters, pb = neighbors[b].positionMeters;
            const auto pc = facility.positionMeters;
            const double area = std::abs((pb.x-pa.x)*(pc.z-pa.z) - (pb.z-pa.z)*(pc.x-pa.x)) / 2.0;
            if (area < rules.triangleMinAreaSquareMeters || distanceSquared(pa, pb) > maxEdge) { continue; }
            DominantTriangle triangle{{}, {pa, pb, pc}, chain, facility.dimension};
            double benefit = rules.aiTriangleScore;
            for (std::size_t i = 0; i < stores.size(); ++i) {
                const auto store = stores.row(i);
                if (store.isActive && store.chain != chain && store.dimension == facility.dimension &&
                    triangleContains(triangle, store.positionMeters)) {
                    benefit += rules.aiEncirclementScore;
                }
            }
            triangleScore = std::max(triangleScore, benefit);
        }
    }
    score += triangleScore;
    for (const auto& triangle : enemyTriangles) {
        if (triangle.chain != chain && triangle.dimension == facility.dimension &&
            triangleContains(triangle, facility.positionMeters)) {
            score -= rules.aiExposurePenalty;
            break;
        }
    }
    return score;
}
}
std::uint32_t opponentPeriodTicks(const std::uint64_t phaseTicks, const Phase1Content& rules) {
    const auto progress = std::min<std::uint64_t>(phaseTicks, rules.aiRampTicks);
    const auto reduction = progress * (rules.aiOpeningPeriodTicks - rules.aiPeriodTicks) / rules.aiRampTicks;
    return rules.aiOpeningPeriodTicks - static_cast<std::uint32_t>(reduction);
}
// AI proposes commands only. Budget, occupancy, source ownership and placement
// rules are checked by exactly the same validator/commit path as player input.
// @implements spec/feature/phase-1-game-loop.md Tick order and ownership
std::optional<PlaceStoreCommand> chooseOpponentPlacement(
    const ChainId chain, const GameState& state, const FacilityTable& facilities,
    const StoreTable& stores, const PopulationCellTable& cells,
    const ChainEconomyTable& economy, const Phase1Content& rules) {
    if (!isFirstPlayableChainId(chain) || !isPlayingPhase(state.phase) || !state.playerChain ||
        chain == *state.playerChain || matchClock(state) < state.nextAiTick[chainIndex(chain)]) { return {}; }
    const auto triangles = buildDominantTriangles(stores, rules);
    std::uint32_t slotLimit = 1;
    // @implements spec/feature/full-campaign-baseline.md Vertical invasion
    if (verticalUnlocked(state)) {
        if (!economy.campaignRules()) {
            throw std::logic_error("vertical AI requires campaign rules");
        }
        slotLimit = economy.campaignRules()->vertical.slots;
    }
    std::optional<PlaceStoreCommand> best;
    double bestScore = -std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < facilities.size(); ++i) {
        const auto facility = facilities.row(i);
        PlaceStoreCommand command{
            {state.completedTicks, CommandSourcePriority::Ai, chainIndex(chain)},
            chain, facility.id, stores.firstEmptySlot(facility.id, slotLimit)};
        if (command.verticalSlot >= slotLimit) { continue; }
        if (validatePlacement(command, state, facilities, stores, economy).failure != PlacementFailure::None) {
            continue;
        }
        const double score = candidateScore(facility, chain, stores, cells, economy, rules, triangles);
        if (!best || score > bestScore ||
            (score == bestScore && facility.id.value() < best->facilityId.value())) {
            best = command;
            bestScore = score;
        }
    }
    return best;
}
}  // namespace konbini::sim
