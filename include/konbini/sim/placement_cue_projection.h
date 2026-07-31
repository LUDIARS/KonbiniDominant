#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "konbini/sim/placement_system.h"
#include "konbini/sim/render_store_placement_cue.h"
#include "konbini/sim/store_table.h"

// @implements spec/feature/npc-conversations-and-placement-feedback.md Store placement choreography

namespace konbini::sim {

// Converts command-system results to presentation records at the tick
// boundary. Failed placement results emit no cue; inconsistent success data is
// a logic error rather than a silent missing animation.
[[nodiscard]] std::vector<RenderStorePlacementCue>
projectStorePlacementCues(
    std::uint64_t completedTick,
    const StoreTable& stores,
    std::span<const PlacementResult> placements);

}  // namespace konbini::sim
