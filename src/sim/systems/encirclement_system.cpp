#include "konbini/sim/encirclement_system.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include "konbini/sim/population_change_system.h"
#include "konbini/sim/skill_system.h"
namespace konbini::sim {
// @implements spec/feature/phase-1-game-loop.md Tick order and ownership
std::vector<Encirclement> resolveEncirclement(
    GameState& state, StoreTable& stores, FacilityTable& facilities,
    ChainEconomyTable& economy, PopulationCellTable& population,
    const std::span<const DominantTriangle> triangles,
    const std::span<const Encirclement> previous, const Phase1Content& rules) {
    const auto delay=[&](ChainId chain) {
        return economy.campaignRules() ? skillCaptureDelay(rules.captureDelayTicks,state,
            economy.campaignRules()->skills,chain) : rules.captureDelayTicks;
    };
    std::vector<Encirclement> threats;
    for (std::size_t i = 0; i < stores.size(); ++i) {
        const auto store = stores.row(i);
        if (!store.isActive) { continue; }
        const auto prior = std::ranges::find_if(previous, [&](const auto& value) {
            return value.target == store.id;
        });
        const DominantTriangle* enclosing = nullptr;
        for (const auto& triangle : triangles) {
            if (triangle.chain == store.chain || triangle.dimension != store.dimension ||
                !triangleContains(triangle, store.positionMeters)) { continue; }
            if (enclosing == nullptr) { enclosing = &triangle; }
            if (prior != previous.end() && prior->attacker == triangle.chain &&
                prior->triangleStores == triangle.stores) {
                enclosing = &triangle;
                break;
            }
        }
        if (enclosing == nullptr) { continue; }
        // The detection tick is the first continuously enclosed tick. Starting
        // at zero would make a configured 30-tick capture take 31 ticks.
        Encirclement threat{store.id, enclosing->chain, enclosing->stores, 1};
        if (prior != previous.end() && prior->attacker == threat.attacker &&
            prior->triangleStores == threat.triangleStores) {
            threat.elapsedTicks = std::min(prior->elapsedTicks, delay(store.chain) - 1) + 1;
        }
        threats.push_back(threat);
    }
    std::sort(threats.begin(), threats.end(), [](const auto& a, const auto& b) {
        return a.target.value() < b.target.value();
    });
    // Decide every mature capture before mutating a store. Mutual encirclement
    // therefore resolves simultaneously, independent of table iteration order.
    for (const auto& threat : threats) {
        const auto index = stores.find(threat.target);
        if (!index) { throw std::logic_error("encircled store disappeared"); }
        if (threat.elapsedTicks < delay(stores.row(*index).chain)) { continue; }
        const auto store = stores.row(*index);
        if (!stores.deactivate(store.id) ||
            !facilities.setState(store.facilityId, stores.hasActiveStoreAt(store.facilityId)
                ? FacilityState::Replaced : FacilityState::Destroyed)) {
            throw std::logic_error("encirclement could not remove its store");
        }
        economy.removeStore(store.chain);
        if (store.verticalSlot == 0) { applyPopulationLoss(population, store.facilityId, rules.destructionPopulationLossPercent); }
        auto& destroyed = state.destroyedStores[chainIndex(threat.attacker)];
        if (destroyed == std::numeric_limits<std::uint32_t>::max()) {
            throw std::overflow_error("destroyed store counter overflow");
        }
        ++destroyed;
    }
    // A vertex destroyed in this same pass also cancels all its surviving
    // warnings. They cannot outlive the actual triangle in the render snapshot.
    std::erase_if(threats, [&](const auto& threat) {
        const auto target = stores.find(threat.target);
        if (!target || !stores.row(*target).isActive) { return true; }
        return std::ranges::any_of(threat.triangleStores, [&](const auto id) {
            const auto index = stores.find(id);
            return !index || !stores.row(*index).isActive;
        });
    });
    return threats;
}
}  // namespace konbini::sim
