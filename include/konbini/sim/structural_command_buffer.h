#pragma once

#include <cstddef>
#include <vector>

#include "konbini/sim/placement_system.h"

// @implements spec/data/world-state.md Structural commands

namespace konbini::sim {

class StructuralCommandBuffer {
public:
    void enqueue(PlaceStoreCommand command);

    [[nodiscard]] std::vector<PlacementResult> commit(
        const GameState& state, FacilityTable& facilities, StoreTable& stores,
        ChainEconomyTable& economy, GenerationalIdPool<StoreId>& storeIds);

    [[nodiscard]] std::size_t pendingCount() const noexcept;

private:
    std::vector<PlaceStoreCommand> placements_;
};

}  // namespace konbini::sim
