#include "konbini/sim/placement_cue_projection.h"

#include <optional>
#include <stdexcept>

// @implements spec/feature/npc-conversations-and-placement-feedback.md Store placement choreography
// @implements spec/interface/visia-presentation.md Store placement sampler

namespace konbini::sim {

// @implements spec/feature/npc-conversations-and-placement-feedback.md Store placement choreography
// @implements spec/interface/visia-presentation.md Store placement sampler
std::vector<RenderStorePlacementCue> projectStorePlacementCues(
    const std::uint64_t completedTick,
    const StoreTable& stores,
    const std::span<const PlacementResult> placements) {
    std::vector<RenderStorePlacementCue> cues;
    cues.reserve(placements.size());
    for (const PlacementResult& placement : placements) {
        if (placement.failure != PlacementFailure::None) {
            if (placement.placedStore.has_value()) {
                throw std::logic_error(
                    "failed placement unexpectedly contains a placed store");
            }
            continue;
        }
        if (!placement.placedStore.has_value()) {
            throw std::logic_error(
                "successful placement is missing its placed store");
        }
        const std::optional<std::size_t> storeIndex =
            stores.find(*placement.placedStore);
        if (!storeIndex.has_value()) {
            throw std::logic_error(
                "successful placement references a missing store");
        }
        const StoreRow store = stores.row(*storeIndex);
        if (!store.isActive || store.id != *placement.placedStore ||
            store.facilityId != placement.command.facilityId ||
            store.chain != placement.command.chain ||
            !isFinite(store.positionMeters)) {
            throw std::logic_error(
                "successful placement does not match its active store row");
        }
        cues.push_back({
            .storeId = store.id,
            .targetPositionMeters = store.positionMeters,
            .targetYawDegrees = 0.0,
            .completedTick = completedTick,
        });
    }
    return cues;
}

}  // namespace konbini::sim
