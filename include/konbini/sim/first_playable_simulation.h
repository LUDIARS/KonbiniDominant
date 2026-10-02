#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "konbini/sim/canonical_snapshot.h"
#include "konbini/sim/command_queue.h"
#include "konbini/sim/pedestrian_path_table.h"
#include "konbini/sim/placement_system.h"
#include "konbini/sim/population_initializer.h"
#include "konbini/sim/render_snapshot.h"
#include "konbini/sim/select_chain_system.h"
#include "konbini/sim/world_entity_ids.h"
#include "konbini/sim/encirclement.h"
#include "konbini/sim/campaign_command_system.h"

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
    std::vector<CampaignCommandResult> campaignActions;
    bool economyCollected = false;
    CanonicalSnapshot canonical;
    std::shared_ptr<const RenderSnapshot> render;
};

class FirstPlayableSimulation {
public:
    // `pedestrianPaths` is the read-only walking graph for resident
    // presentation. It is not simulation state: it is never staged, saved,
    // or included in the canonical snapshot.
    FirstPlayableSimulation(FirstPlayableContent content,
                            FacilityTable facilities,
                            WorldEntityIds identities,
                            std::optional<PedestrianPathTable> pedestrianPaths =
                                std::nullopt);

    void submit(PlayerCommand command);
    [[nodiscard]] CompletedTick completeNextTick();

    [[nodiscard]] const GameState& state() const noexcept;
    [[nodiscard]] const FacilityTable& facilities() const noexcept;
    [[nodiscard]] const StoreTable& stores() const noexcept;
    [[nodiscard]] const PopulationCellTable& populationCells() const noexcept;
    [[nodiscard]] const ChainEconomyTable& economy() const noexcept;
    [[nodiscard]] const PedestrianPathTable* pedestrianPaths() const noexcept;

private:
    FirstPlayableContent content_;
    GameState state_;
    FacilityTable facilities_;
    StoreTable stores_;
    PopulationCellTable populationCells_;
    ChainEconomyTable economy_;
    WorldEntityIds identities_;
    CommandQueue commands_;
    std::vector<DominantTriangle> triangles_;
    std::vector<Encirclement> encirclements_;
    std::optional<PedestrianPathTable> pedestrianPaths_;
};

}  // namespace konbini::sim
