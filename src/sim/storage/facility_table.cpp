#include "konbini/sim/facility_table.h"

#include <stdexcept>

#include "konbini/sim/dense_column.h"

// @implements spec/data/world-state.md FacilityTable

namespace konbini::sim {

// @implements spec/data/world-state.md FacilityTable
void FacilityTable::append(const FacilityRow& value) {
    if (!value.id.isValid() || !value.figmentumKey.isValid() ||
        !isFinite(value.positionMeters) || !isFiniteAndOrdered(value.boundsMeters)) {
        throw std::invalid_argument("invalid facility row");
    }
    // sparse slot は entity index 単位なので、generation 違いでも占有済みなら
    // 拒否する。row 削除と sparse slot の解放は structural command buffer
    // (world-state.md#structural-commands) 側の後続 task で入る。それまで、
    // table 常駐 entity の ID を pool へ release して index を再利用しない。
    const std::size_t sparseIndex = value.id.value().index;
    if (sparseIndex < sparseIndices_.size() &&
        sparseIndices_[sparseIndex] != kMissing) {
        throw std::invalid_argument("duplicate facility id");
    }

    const std::size_t nextSize = ids_.size() + 1;
    reserveForAppend(ids_, nextSize);
    reserveForAppend(figmentumKeys_, nextSize);
    reserveForAppend(dimensions_, nextSize);
    reserveForAppend(positionsMeters_, nextSize);
    reserveForAppend(boundsMeters_, nextSize);
    reserveForAppend(isBuildable_, nextSize);
    reserveForAppend(states_, nextSize);
    if (sparseIndices_.size() <= sparseIndex) {
        sparseIndices_.resize(sparseIndex + 1, kMissing);
    }

    const std::size_t denseIndex = ids_.size();
    ids_.push_back(value.id);
    figmentumKeys_.push_back(value.figmentumKey);
    dimensions_.push_back(value.dimension);
    positionsMeters_.push_back(value.positionMeters);
    boundsMeters_.push_back(value.boundsMeters);
    isBuildable_.push_back(value.isBuildable ? 1U : 0U);
    states_.push_back(value.state);
    sparseIndices_[sparseIndex] = denseIndex;
}

// @implements spec/data/world-state.md FacilityTable
std::size_t FacilityTable::size() const noexcept {
    return ids_.size();
}

// @implements spec/data/world-state.md FacilityTable
FacilityRow FacilityTable::row(const std::size_t index) const {
    if (index >= size()) {
        throw std::out_of_range("facility dense index is out of range");
    }
    return {
        .id = ids_[index],
        .figmentumKey = figmentumKeys_[index],
        .dimension = dimensions_[index],
        .positionMeters = positionsMeters_[index],
        .boundsMeters = boundsMeters_[index],
        .isBuildable = isBuildable_[index] != 0,
        .state = states_[index],
    };
}

// ID→dense index の sparse lookup。generation 違いの stale handle は
// `ids_[index] != id` で落とし、別 entity の row へ解決しない。
// @implements spec/data/world-state.md FacilityTable
std::optional<std::size_t> FacilityTable::find(const FacilityId id) const noexcept {
    if (!id.isValid() || id.value().index >= sparseIndices_.size()) {
        return std::nullopt;
    }
    const std::size_t index = sparseIndices_[id.value().index];
    if (index == kMissing || index >= ids_.size() || ids_[index] != id) {
        return std::nullopt;
    }
    return index;
}

// @implements spec/data/world-state.md FacilityTable
bool FacilityTable::setState(const FacilityId id,
                             const FacilityState state) noexcept {
    const std::optional<std::size_t> index = find(id);
    if (!index.has_value()) {
        return false;
    }
    states_[*index] = state;
    return true;
}

}  // namespace konbini::sim
