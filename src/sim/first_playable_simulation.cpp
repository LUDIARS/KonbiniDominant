#include "konbini/sim/first_playable_simulation.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "konbini/sim/economy_system.h"
#include "konbini/sim/placement_cue_projection.h"
#include "konbini/sim/structural_command_buffer.h"
#include "konbini/sim/zoc_system.h"

// @implements spec/design.md 5. Tick と決定性
// @implements spec/plan/tasks/first-playable.md Simulation
// @implements spec/feature/npc-conversations-and-placement-feedback.md Determinism and ownership

namespace konbini::sim {

// @implements spec/plan/tasks/first-playable.md Simulation
// @implements spec/data/content-schema.md Validation
FirstPlayableSimulation::FirstPlayableSimulation(FirstPlayableContent content,
                                                 FacilityTable facilities,
                                                 WorldEntityIds identities)
    : content_(std::move(content)),
      facilities_(std::move(facilities)),
      economy_(content_),
      identities_(std::move(identities)) {
    validateFirstPlayableContent(content_);
    initializePopulationCells(facilities_,
                              content_,
                              state_.worldSeed,
                              populationCells_,
                              identities_.populationCells());
}

// 過ぎた tick を狙う command は queue へ入れない。受理してから
// `takeForTick` で捨てると、command stream が同じでも tick 境界がずれる。
// @implements spec/design.md 5. Tick と決定性
void FirstPlayableSimulation::submit(PlayerCommand command) {
    if (state_.completedTicks > commandOrder(command).targetTick) {
        throw std::invalid_argument(
            "cannot submit a command for an already completed tick");
    }
    commands_.push(std::move(command));
}

// @implements spec/design.md 5. Tick と決定性
// @implements spec/plan/tasks/first-playable.md Simulation
CompletedTick FirstPlayableSimulation::completeNextTick() {
    if (state_.completedTicks == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error("simulation tick counter exhausted");
    }

    // A tick is one transaction. Commands, entity ids, tables, economy, and
    // both snapshots are prepared against staged copies. Any validation or
    // overflow exception therefore leaves the authoritative simulation and
    // its pending command queue untouched for deterministic retry.
    GameState stagedState = state_;
    FacilityTable stagedFacilities = facilities_;
    StoreTable stagedStores = stores_;
    PopulationCellTable stagedPopulationCells = populationCells_;
    ChainEconomyTable stagedEconomy = economy_;
    WorldEntityIds stagedIdentities = identities_;
    CommandQueue stagedCommands = commands_;
    StructuralCommandBuffer structuralCommands;

    CompletedTick result;
    const std::uint64_t targetTick = stagedState.completedTicks;
    std::vector<PlayerCommand> commands =
        stagedCommands.takeForTick(targetTick);
    stagedEconomy.resetTickMetrics();
    for (const PlayerCommand& command : commands) {
        std::visit(
            [&result,
             &stagedState,
             &stagedFacilities,
             &stagedStores,
             &stagedEconomy,
             &structuralCommands](const auto& value) {
                using Command = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Command, SelectChainCommand>) {
                    result.chainSelections.push_back({
                        .command = value,
                        .failure =
                            applySelectChain(value, stagedState, stagedEconomy),
                    });
                } else {
                    const PlacementDecision decision = validatePlacement(
                        value,
                        stagedState,
                        stagedFacilities,
                        stagedStores,
                        stagedEconomy);
                    if (decision.failure == PlacementFailure::None) {
                        structuralCommands.enqueue(value);
                    } else {
                        result.placements.push_back({
                            .command = value,
                            .failure = decision.failure,
                        });
                    }
                }
            },
            command);
    }

    std::vector<PlacementResult> committed = structuralCommands.commit(
        stagedState,
        stagedFacilities,
        stagedStores,
        stagedEconomy,
        stagedIdentities.stores());
    result.placements.insert(result.placements.end(),
                             std::make_move_iterator(committed.begin()),
                             std::make_move_iterator(committed.end()));
    std::sort(
        result.placements.begin(),
        result.placements.end(),
        [](const PlacementResult& left, const PlacementResult& right) {
            return commandLess(PlayerCommand(left.command),
                               PlayerCommand(right.command));
        });

    (void)assignNearestStores(
        stagedStores, stagedPopulationCells, stagedEconomy);
    ++stagedState.completedTicks;
    result.economyCollected = collectEconomyForCompletedTick(
        stagedState.completedTicks,
        content_.simulation.economyPeriodTicks,
        stagedStores,
        stagedEconomy);
    result.completedTicks = stagedState.completedTicks;
    result.canonical = makeCanonicalSnapshot(stagedState,
                                             content_,
                                             stagedFacilities,
                                             stagedStores,
                                             stagedPopulationCells,
                                             stagedEconomy);
    const std::vector<RenderStorePlacementCue> placementCues =
        projectStorePlacementCues(
            stagedState.completedTicks,
            stagedStores,
            result.placements);
    result.render = makeRenderSnapshot(stagedState,
                                       content_,
                                       stagedFacilities,
                                       stagedStores,
                                       stagedPopulationCells,
                                       stagedEconomy,
                                       placementCues);

    static_assert(
        std::is_nothrow_move_assignable_v<GameState> &&
            std::is_nothrow_move_assignable_v<FacilityTable> &&
            std::is_nothrow_move_assignable_v<StoreTable> &&
            std::is_nothrow_move_assignable_v<PopulationCellTable> &&
            std::is_nothrow_move_assignable_v<ChainEconomyTable> &&
            std::is_nothrow_move_assignable_v<WorldEntityIds> &&
            std::is_nothrow_move_assignable_v<CommandQueue>,
        "authoritative tick commit must not throw");
    state_ = std::move(stagedState);
    facilities_ = std::move(stagedFacilities);
    stores_ = std::move(stagedStores);
    populationCells_ = std::move(stagedPopulationCells);
    economy_ = std::move(stagedEconomy);
    identities_ = std::move(stagedIdentities);
    commands_ = std::move(stagedCommands);
    return result;
}

// 以下の accessor は commit 済みの authoritative state だけを read-only で
// 見せる。tick 途中の staged copy は外へ出さない。
// @implements spec/plan/tasks/first-playable.md Simulation
const GameState& FirstPlayableSimulation::state() const noexcept {
    return state_;
}

const FacilityTable& FirstPlayableSimulation::facilities() const noexcept {
    return facilities_;
}

const StoreTable& FirstPlayableSimulation::stores() const noexcept {
    return stores_;
}

const PopulationCellTable&
FirstPlayableSimulation::populationCells() const noexcept {
    return populationCells_;
}

const ChainEconomyTable& FirstPlayableSimulation::economy() const noexcept {
    return economy_;
}

}  // namespace konbini::sim
