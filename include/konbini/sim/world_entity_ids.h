#pragma once

#include "konbini/sim/entity_id.h"

// @implements spec/data/world-state.md ID

namespace konbini::sim {

// entity 種別ごとに独立した index space を持つ。pool を共有すると、
// 型が違うだけで同一 index/generation の handle が別種 entity へ解決される。
class WorldEntityIds {
public:
    // @implements spec/data/world-state.md ID
    [[nodiscard]] GenerationalIdPool<FacilityId>& facilities() noexcept {
        return facilityIds_;
    }

    // @implements spec/data/world-state.md ID
    [[nodiscard]] GenerationalIdPool<StoreId>& stores() noexcept {
        return storeIds_;
    }

    // @implements spec/data/world-state.md ID
    [[nodiscard]] GenerationalIdPool<PopulationCellId>& populationCells() noexcept {
        return populationCellIds_;
    }

private:
    GenerationalIdPool<FacilityId> facilityIds_;
    GenerationalIdPool<StoreId> storeIds_;
    GenerationalIdPool<PopulationCellId> populationCellIds_;
};

}  // namespace konbini::sim
