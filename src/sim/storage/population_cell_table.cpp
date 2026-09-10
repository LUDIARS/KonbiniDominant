#include "konbini/sim/population_cell_table.h"

#include <stdexcept>

#include "konbini/sim/dense_column.h"

// @implements spec/data/world-state.md PopulationCellTable

namespace konbini::sim {

namespace {

// assignment は store と chain を対で持つ。片方だけの assignment は
// `hasAssignment_` の 1 bit へ潰せず、row() が解決できない対を返す。
// 無効な `StoreId` (generation 0) も `StoreTable::find` で解決できないので、
// assigned 済みとして受け入れない。assigned だが参照先が引けない cell は
// 未割当としても集計されず、人口が黙って消える。
// @implements spec/data/world-state.md PopulationCellTable
[[nodiscard]] bool isValidAssignment(
    const std::optional<StoreId>& store,
    const std::optional<ChainId>& chain) noexcept {
    if (store.has_value() != chain.has_value()) {
        return false;
    }
    if (!store.has_value()) {
        return true;
    }
    return store->isValid() && isSimulationChainId(*chain);
}

}  // namespace

// @implements spec/data/world-state.md PopulationCellTable
void PopulationCellTable::append(const PopulationCellRow& value) {
    if (!value.id.isValid() || !value.facilityId.isValid() ||
        !isFinite(value.positionMeters) ||
        !isValidAssignment(value.assignedStore, value.preferredChain) ||
        (value.capacity != 0 && value.population > value.capacity)) {
        throw std::invalid_argument("invalid population cell row");
    }

    const std::size_t nextSize = ids_.size() + 1;
    reserveForAppend(ids_, nextSize);
    reserveForAppend(facilityIds_, nextSize);
    reserveForAppend(dimensions_, nextSize);
    reserveForAppend(positionsMeters_, nextSize);
    reserveForAppend(populations_, nextSize);
    reserveForAppend(capacities_, nextSize);
    reserveForAppend(assignedStores_, nextSize);
    reserveForAppend(preferredChains_, nextSize);
    reserveForAppend(hasAssignment_, nextSize);

    ids_.push_back(value.id);
    facilityIds_.push_back(value.facilityId);
    dimensions_.push_back(value.dimension);
    positionsMeters_.push_back(value.positionMeters);
    populations_.push_back(value.population);
    capacities_.push_back(value.capacity == 0 ? value.population : value.capacity);
    assignedStores_.push_back(value.assignedStore.value_or(StoreId{}));
    preferredChains_.push_back(value.preferredChain.value_or(ChainId::Losan));
    hasAssignment_.push_back(value.assignedStore.has_value() ? 1U : 0U);
}

// @implements spec/data/world-state.md PopulationCellTable
std::size_t PopulationCellTable::size() const noexcept {
    return ids_.size();
}

// @implements spec/data/world-state.md PopulationCellTable
PopulationCellRow PopulationCellTable::row(const std::size_t index) const {
    if (index >= size()) {
        throw std::out_of_range("population cell dense index is out of range");
    }
    const bool hasAssignment = hasAssignment_[index] != 0;
    return {
        .id = ids_[index],
        .facilityId = facilityIds_[index],
        .dimension = dimensions_[index],
        .positionMeters = positionsMeters_[index],
        .population = populations_[index],
        .assignedStore =
            hasAssignment ? std::optional<StoreId>(assignedStores_[index])
                          : std::nullopt,
        .preferredChain =
            hasAssignment ? std::optional<ChainId>(preferredChains_[index])
                          : std::nullopt,
        .capacity = capacities_[index],
    };
}

// @implements spec/data/world-state.md PopulationCellTable
void PopulationCellTable::assign(const std::size_t denseIndex,
                                 const std::optional<StoreId> store,
                                 const std::optional<ChainId> chain) {
    if (denseIndex >= size()) {
        throw std::out_of_range("population cell dense index is out of range");
    }
    if (!isValidAssignment(store, chain)) {
        throw std::invalid_argument(
            "population assignment requires a valid store and chain pair");
    }
    assignedStores_[denseIndex] = store.value_or(StoreId{});
    preferredChains_[denseIndex] = chain.value_or(ChainId::Losan);
    hasAssignment_[denseIndex] = store.has_value() ? 1U : 0U;
}

void PopulationCellTable::setPopulation(const std::size_t denseIndex,
                                        const std::uint32_t population) {
    if (denseIndex >= size() || population > capacities_[denseIndex]) {
        throw std::invalid_argument("population exceeds cell capacity");
    }
    populations_[denseIndex] = population;
}
}  // namespace konbini::sim
