#pragma once

#include <cstdint>

#include "konbini/sim/entity_id.h"
#include "konbini/sim/math_types.h"

// @implements spec/feature/npc-conversations-and-placement-feedback.md Store placement choreography
// @implements spec/interface/visia-presentation.md Store placement sampler

namespace konbini::sim {

// A successful placement produces one transient cue. Its transform is copied
// from the authoritative StoreTable; render adapters may animate it without
// changing simulation state.
struct RenderStorePlacementCue {
    StoreId storeId{};
    Vec3 targetPositionMeters{};
    double targetYawDegrees = 0.0;
    std::uint64_t completedTick = 0;
};

}  // namespace konbini::sim
