#include "konbini/sim/store_table.h"

#include <cmath>
#include <limits>
#include <stdexcept>

#include "konbini/sim/dense_column.h"

// @implements spec/data/world-state.md StoreTable

namespace konbini::sim {

// @implements spec/data/world-state.md StoreTable
void StoreTable::append(const StoreRow& value) {
    if (!value.id.isValid() || !value.facilityId.isValid() ||
        !isFirstPlayableChainId(value.chain) ||
        !isFinite(value.positionMeters) || !std::isfinite(value.zocRadiusMeters) ||
        value.zocRadiusMeters <= 0.0) {
        throw std::invalid_argument("invalid store row");
    }
    // sparse slot は entity index 単位なので、generation 違いでも占有済みなら
    // 拒否する。row 削除と sparse slot の解放は structural command buffer
    // (world-state.md#structural-commands) 側の後続 task で入る。それまで、
    // table 常駐 entity の ID を pool へ release して index を再利用しない。
    const std::size_t sparseIndex = value.id.value().index;
    if (sparseIndex < sparseIndices_.size() &&
        sparseIndices_[sparseIndex] != kMissing) {
        throw std::invalid_argument("duplicate store id");
    }

    const std::size_t nextSize = ids_.size() + 1;
    reserveForAppend(ids_, nextSize);
    reserveForAppend(facilityIds_, nextSize);
    reserveForAppend(chains_, nextSize);
    reserveForAppend(dimensions_, nextSize);
    reserveForAppend(positionsMeters_, nextSize);
    reserveForAppend(zocRadiiMeters_, nextSize);
    reserveForAppend(capturedPopulations_, nextSize);
    reserveForAppend(isActive_, nextSize);
    if (sparseIndices_.size() <= sparseIndex) {
        sparseIndices_.resize(sparseIndex + 1, kMissing);
    }

    const std::size_t denseIndex = ids_.size();
    ids_.push_back(value.id);
    facilityIds_.push_back(value.facilityId);
    chains_.push_back(value.chain);
    dimensions_.push_back(value.dimension);
    positionsMeters_.push_back(value.positionMeters);
    zocRadiiMeters_.push_back(value.zocRadiusMeters);
    capturedPopulations_.push_back(value.capturedPopulation);
    isActive_.push_back(value.isActive ? 1U : 0U);
    sparseIndices_[sparseIndex] = denseIndex;
}

// @implements spec/data/world-state.md StoreTable
std::size_t StoreTable::size() const noexcept {
    return ids_.size();
}

// @implements spec/data/world-state.md StoreTable
StoreRow StoreTable::row(const std::size_t index) const {
    if (index >= size()) {
        throw std::out_of_range("store dense index is out of range");
    }
    return {
        .id = ids_[index],
        .facilityId = facilityIds_[index],
        .chain = chains_[index],
        .dimension = dimensions_[index],
        .positionMeters = positionsMeters_[index],
        .zocRadiusMeters = zocRadiiMeters_[index],
        .capturedPopulation = capturedPopulations_[index],
        .isActive = isActive_[index] != 0,
    };
}

// ID→dense index の sparse lookup。generation 違いの stale handle は
// `ids_[index] != id` で落とし、別 entity の row へ解決しない。
// @implements spec/data/world-state.md StoreTable
std::optional<std::size_t> StoreTable::find(const StoreId id) const noexcept {
    if (!id.isValid() || id.value().index >= sparseIndices_.size()) {
        return std::nullopt;
    }
    const std::size_t index = sparseIndices_[id.value().index];
    if (index == kMissing || index >= ids_.size() || ids_[index] != id) {
        return std::nullopt;
    }
    return index;
}

// facilityId 側の逆引き index はまだ無いので dense column を線形走査する。
// 走査対象は active flag と facility ID の2 column だけなので、
// spatial index (world-state.md#spatial-index) が入るまではこれで足りる。
// @implements spec/data/world-state.md StoreTable
bool StoreTable::hasActiveStoreAt(const FacilityId facilityId) const noexcept {
    for (std::size_t index = 0; index < size(); ++index) {
        if (isActive_[index] != 0 && facilityIds_[index] == facilityId) {
            return true;
        }
    }
    return false;
}

// @implements spec/data/world-state.md StoreTable
void StoreTable::clearCapturedPopulation() noexcept {
    for (std::uint64_t& value : capturedPopulations_) {
        value = 0;
    }
}

// @implements spec/data/world-state.md StoreTable
void StoreTable::addCapturedPopulation(const std::size_t denseIndex,
                                       const std::uint32_t population) {
    if (denseIndex >= size()) {
        throw std::out_of_range("store dense index is out of range");
    }
    if (capturedPopulations_[denseIndex] >
        std::numeric_limits<std::uint64_t>::max() - population) {
        throw std::overflow_error("captured population overflow");
    }
    capturedPopulations_[denseIndex] += population;
}

}  // namespace konbini::sim
