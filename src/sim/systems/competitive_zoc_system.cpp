#include "konbini/sim/competitive_zoc_system.h"
#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <stdexcept>
namespace konbini::sim {
namespace {
double distanceSquared(const Vec3 a, const Vec3 b) noexcept {
    return (a.x-b.x)*(a.x-b.x) + (a.z-b.z)*(a.z-b.z);
}
struct Influence {
    std::uint64_t score = 0;
    std::optional<std::size_t> nearest;
    double distance = std::numeric_limits<double>::max();
};
void considerStore(Influence& influence, const StoreTable& stores,
                   const std::size_t index, const Vec3 position) {
    const auto store = stores.row(index);
    const double distance = distanceSquared(position, store.positionMeters);
    if (!influence.nearest || distance < influence.distance ||
        (distance == influence.distance &&
         store.id.value() < stores.row(*influence.nearest).id.value())) {
        influence.nearest = index;
        influence.distance = distance;
    }
}
}  // namespace

// One authoritative owner and one receiving store per cell prevents duplicate
// income from overlapping circles/triangles. The prior owner wins score ties.
// @implements spec/feature/phase-1-game-loop.md Match contract
void assignCompetitiveCustomers(StoreTable& stores, PopulationCellTable& cells,
                                ChainEconomyTable& economy,
                                const std::span<const DominantTriangle> triangles,
                                const Phase1Content& rules, const bool useFaith, const SkillInfluence& skills) {
    stores.clearCapturedPopulation();
    std::array<std::uint64_t, kSimulationChainCount> population{};
    for (std::size_t cellIndex = 0; cellIndex < cells.size(); ++cellIndex) {
        const auto cell = cells.row(cellIndex);
        std::array<Influence, kSimulationChainCount> influence{};
        for (std::size_t i = 0; i < stores.size(); ++i) {
            const auto store = stores.row(i);
            if (!store.isActive || store.dimension != cell.dimension ||
                distanceSquared(store.positionMeters, cell.positionMeters) >
                    store.zocRadiusMeters * store.zocRadiusMeters) { continue; }
            auto& score = influence[chainIndex(store.chain)];
            score.score += useFaith && store.faith == 100 ? 2U : 1U;
            considerStore(score, stores, i, cell.positionMeters);
        }
        // Periodic skills only reach residents inside a live player store circle.
        std::optional<std::size_t> waveStore;
        if(skills.playerChain>=0 && skills.playerChain<static_cast<int>(kFirstPlayableChainCount)) {
            auto& player=influence[static_cast<std::size_t>(skills.playerChain)];
            if(player.nearest) {
                player.score+=skills.bonus;
                if(skills.mindWave) waveStore=player.nearest;
            }
        }
        std::array<bool, kSimulationChainCount> triangleApplied{};
        for (const auto& triangle : triangles) {
            if (triangle.dimension != cell.dimension || !triangleContains(triangle, cell.positionMeters)) {
                continue;
            }
            const auto chain = chainIndex(triangle.chain);
            if (!triangleApplied[chain]) {
                influence[chain].score += rules.triangleInfluence;
                triangleApplied[chain] = true;
            }
            for (const auto id : triangle.stores) {
                const auto storeIndex = stores.find(id);
                if (!storeIndex || !stores.row(*storeIndex).isActive) {
                    throw std::logic_error("customer assignment received an invalid triangle");
                }
                considerStore(influence[chain], stores, *storeIndex, cell.positionMeters);
            }
        }
        if(waveStore) {
            const auto store=stores.row(*waveStore);
            cells.assign(cellIndex,store.id,store.chain);
            stores.addCapturedPopulation(*waveStore,cell.population);
            auto& total=population[chainIndex(store.chain)];
            if(total>std::numeric_limits<std::uint64_t>::max()-cell.population)
                throw std::overflow_error("mind wave population overflow");
            total+=cell.population;
            continue;
        }
        std::uint64_t maximum = 0;
        for (const auto& score : influence) { maximum = std::max(maximum, score.score); }
        if (maximum == 0) {
            cells.assign(cellIndex, std::nullopt, std::nullopt);
            continue;
        }
        std::optional<std::size_t> winner;
        if (cell.preferredChain && influence[chainIndex(*cell.preferredChain)].score == maximum) {
            winner = chainIndex(*cell.preferredChain);
        } else {
            for (std::size_t chain = 0; chain < influence.size(); ++chain) {
                const auto& candidate = influence[chain];
                if (candidate.score != maximum || !candidate.nearest) { continue; }
                if (!winner || candidate.distance < influence[*winner].distance ||
                    (candidate.distance == influence[*winner].distance &&
                     stores.row(*candidate.nearest).id.value() <
                         stores.row(*influence[*winner].nearest).id.value())) {
                    winner = chain;
                }
            }
        }
        if (!winner || !influence[*winner].nearest) {
            throw std::logic_error("positive influence has no receiving store");
        }
        const auto index = *influence[*winner].nearest;
        const auto store = stores.row(index);
        cells.assign(cellIndex, store.id, store.chain);
        stores.addCapturedPopulation(index, cell.population);
        if (population[*winner] > std::numeric_limits<std::uint64_t>::max() - cell.population) {
            throw std::overflow_error("chain customer population overflow");
        }
        population[*winner] += cell.population;
    }
    for (std::size_t chain = 0; chain < population.size(); ++chain) {
        economy.setCustomerShare(static_cast<ChainId>(chain), population[chain]);
    }
}
}  // namespace konbini::sim
