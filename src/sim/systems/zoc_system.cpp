#include "konbini/sim/zoc_system.h"

#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

// @implements spec/feature/phase-1-dominant-triangle.md ZOC
// @implements spec/feature/economy-and-population.md 顧客化

namespace konbini::sim {

namespace {

double distanceSquaredXZ(const Vec3 left, const Vec3 right) noexcept {
    const double deltaX = left.x - right.x;
    const double deltaZ = left.z - right.z;
    return deltaX * deltaX + deltaZ * deltaZ;
}

bool storeIdLess(const StoreId left, const StoreId right) noexcept {
    if (left.value().index != right.value().index) {
        return left.value().index < right.value().index;
    }
    return left.value().generation < right.value().generation;
}

}  // namespace

// BASE-P1-ZOC-01: XZ 平面の円形 range で判定する。距離が同点の cell は
// StoreId 昇順で解決し、store の dense 順が変わっても割当が揺れないようにする
// (spatial index と hysteresis は TBD-ZOC-01 / 顧客化 の後続 task)。
// @implements spec/feature/phase-1-dominant-triangle.md ZOC
// @implements spec/feature/economy-and-population.md 顧客化
ZocSummary assignNearestStores(StoreTable& stores,
                               PopulationCellTable& populationCells,
                               ChainEconomyTable& economy) {
    StoreTable stagedStores = stores;
    PopulationCellTable stagedPopulationCells = populationCells;
    ChainEconomyTable stagedEconomy = economy;
    stagedStores.clearCapturedPopulation();
    ZocSummary summary;

    for (std::size_t populationIndex = 0;
         populationIndex < stagedPopulationCells.size();
         ++populationIndex) {
        const PopulationCellRow cell =
            stagedPopulationCells.row(populationIndex);
        std::optional<std::size_t> nearestIndex;
        double nearestDistanceSquared = 0.0;
        StoreId nearestStore;

        for (std::size_t storeIndex = 0;
             storeIndex < stagedStores.size();
             ++storeIndex) {
            const StoreRow store = stagedStores.row(storeIndex);
            if (!store.isActive || store.dimension != cell.dimension) {
                continue;
            }
            const double distanceSquared =
                distanceSquaredXZ(store.positionMeters, cell.positionMeters);
            const double radiusSquared =
                store.zocRadiusMeters * store.zocRadiusMeters;
            if (distanceSquared > radiusSquared) {
                continue;
            }
            if (!nearestIndex.has_value() ||
                distanceSquared < nearestDistanceSquared ||
                (distanceSquared == nearestDistanceSquared &&
                 storeIdLess(store.id, nearestStore))) {
                nearestIndex = storeIndex;
                nearestDistanceSquared = distanceSquared;
                nearestStore = store.id;
            }
        }

        if (!nearestIndex.has_value()) {
            if (summary.unassignedCellCount ==
                std::numeric_limits<std::uint32_t>::max()) {
                throw std::overflow_error("unassigned population cell overflow");
            }
            stagedPopulationCells.assign(
                populationIndex, std::nullopt, std::nullopt);
            ++summary.unassignedCellCount;
            continue;
        }

        const StoreRow store = stagedStores.row(*nearestIndex);
        stagedPopulationCells.assign(
            populationIndex, store.id, store.chain);
        stagedStores.addCapturedPopulation(*nearestIndex, cell.population);
        std::uint64_t& chainPopulation =
            summary.capturedPopulation[chainIndex(store.chain)];
        if (chainPopulation >
            std::numeric_limits<std::uint64_t>::max() - cell.population) {
            throw std::overflow_error("chain captured population overflow");
        }
        chainPopulation += cell.population;
    }

    for (std::size_t index = 0; index < kFirstPlayableChainCount; ++index) {
        stagedEconomy.setCustomerShare(
            static_cast<ChainId>(index),
            summary.capturedPopulation[index]);
    }
    stores = std::move(stagedStores);
    populationCells = std::move(stagedPopulationCells);
    economy = std::move(stagedEconomy);
    return summary;
}

}  // namespace konbini::sim
