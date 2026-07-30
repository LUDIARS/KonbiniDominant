#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "konbini/sim/canonical_snapshot.h"
#include "konbini/sim/command_queue.h"
#include "konbini/sim/placement_system.h"
#include "konbini/sim/population_initializer.h"
#include "konbini/sim/render_snapshot.h"
#include "konbini/sim/select_chain_system.h"
#include "konbini/sim/world_entity_ids.h"

// @implements spec/design.md 5. Tick と決定性
// @implements spec/plan/tasks/first-playable.md Simulation

namespace konbini::sim {

struct SelectChainResult {
    SelectChainCommand command{};
    SelectChainFailure failure = SelectChainFailure::None;
};

struct CompletedTick {
    std::uint64_t completedTicks = 0;
    std::vector<SelectChainResult> chainSelections;
    std::vector<PlacementResult> placements;
    bool economyCollected = false;
    CanonicalSnapshot canonical;
    std::shared_ptr<const RenderSnapshot> render;
};

class FirstPlayableSimulation {
public:
    FirstPlayableSimulation(FirstPlayableContent content,
                            FacilityTable facilities,
                            WorldEntityIds identities);

    void submit(PlayerCommand command);
    [[nodiscard]] CompletedTick completeNextTick();

    [[nodiscard]] const GameState& state() const noexcept;
    [[nodiscard]] const FacilityTable& facilities() const noexcept;
    [[nodiscard]] const StoreTable& stores() const noexcept;
    [[nodiscard]] const PopulationCellTable& populationCells() const noexcept;
    [[nodiscard]] const ChainEconomyTable& economy() const noexcept;

private:
    FirstPlayableContent content_;
    GameState state_;
    FacilityTable facilities_;
    StoreTable stores_;
    PopulationCellTable populationCells_;
    ChainEconomyTable economy_;
    WorldEntityIds identities_;
    CommandQueue commands_;
};

}  // namespace konbini::sim
