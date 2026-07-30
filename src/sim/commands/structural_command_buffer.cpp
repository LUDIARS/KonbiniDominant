#include "konbini/sim/structural_command_buffer.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

// @implements spec/data/world-state.md Structural commands

namespace konbini::sim {

// system scan 中は queue へ積むだけにする (world-state.md#Structural commands)。
// @implements spec/data/world-state.md Structural commands
void StructuralCommandBuffer::enqueue(PlaceStoreCommand command) {
    placements_.push_back(std::move(command));
}

// tick の既定境界で一括適用する。batch 全体を staged copy 上で組み立て、
// 途中の command が throw した場合は authoritative table を差し替えない。
// @implements spec/data/world-state.md Structural commands
std::vector<PlacementResult> StructuralCommandBuffer::commit(
    const GameState& state, FacilityTable& facilities, StoreTable& stores,
    ChainEconomyTable& economy, GenerationalIdPool<StoreId>& storeIds) {
    for (const PlaceStoreCommand& command : placements_) {
        if (command.order.targetTick != state.completedTicks) {
            throw std::invalid_argument(
                "structural placement targets a different simulation tick");
        }
    }
    std::sort(placements_.begin(),
              placements_.end(),
              [](const PlaceStoreCommand& left, const PlaceStoreCommand& right) {
                  return commandLess(PlayerCommand(left), PlayerCommand(right));
              });

    FacilityTable stagedFacilities = facilities;
    StoreTable stagedStores = stores;
    ChainEconomyTable stagedEconomy = economy;
    GenerationalIdPool<StoreId> stagedStoreIds = storeIds;
    std::vector<PlacementResult> results;
    results.reserve(placements_.size());
    for (const PlaceStoreCommand& command : placements_) {
        results.push_back(commitPlacementAtomically(
            command,
            state,
            stagedFacilities,
            stagedStores,
            stagedEconomy,
            stagedStoreIds));
    }

    facilities = std::move(stagedFacilities);
    stores = std::move(stagedStores);
    economy = std::move(stagedEconomy);
    storeIds = std::move(stagedStoreIds);
    placements_.clear();
    return results;
}

std::size_t StructuralCommandBuffer::pendingCount() const noexcept {
    return placements_.size();
}

}  // namespace konbini::sim
