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
#include "konbini/sim/competitive_zoc_system.h"
#include "konbini/sim/encirclement_system.h"
#include "konbini/sim/opponent_ai_system.h"
#include "konbini/sim/phase1_outcome_system.h"
#include "konbini/sim/population_change_system.h"
#include "konbini/sim/campaign_progression.h"
#include "konbini/sim/faith_system.h"
#include "konbini/sim/aion_system.h"
#include "konbini/sim/vertical_placement.h"
#include "konbini/sim/skill_system.h"

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
    state_.competitive = content_.phase1.has_value();
    state_.campaign.enabled = content_.campaign.has_value();
    if (state_.campaign.enabled) { state_.campaign.dimensions.push_back({0, state_.worldSeed}); }
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
    if (state_.phase == GamePhase::Result) {
        throw std::invalid_argument("match has ended; retry before submitting commands");
    }
    if (commandOrder(command).sourcePriority != CommandSourcePriority::Player) {
        throw std::invalid_argument("only the internal opponent system may issue AI commands");
    }
    if (state_.completedTicks > commandOrder(command).targetTick) {
        throw std::invalid_argument(
            "cannot submit a command for an already completed tick");
    }
    commands_.push(std::move(command));
}

// @implements spec/design.md 5. Tick と決定性
// @implements spec/plan/tasks/first-playable.md Simulation
CompletedTick FirstPlayableSimulation::completeNextTick() {
    if (state_.phase == GamePhase::Result) {
        CompletedTick frozen;
        frozen.completedTicks = state_.completedTicks;
        frozen.canonical = makeCanonicalSnapshot(state_, content_, facilities_, stores_,
            populationCells_, economy_, encirclements_);
        frozen.render = makeRenderSnapshot(state_, content_, facilities_, stores_,
            populationCells_, economy_, {}, triangles_, encirclements_);
        return frozen;
    }
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
    auto stagedTriangles = triangles_;
    auto stagedEncirclements = encirclements_;
    CampaignWorld stagedWorld{stagedState, content_, stagedFacilities, stagedStores,
        stagedPopulationCells, stagedEconomy, stagedIdentities};

    const bool choosingSkill=stagedState.campaign.enabled && stagedState.campaign.skills.pending;
    CompletedTick result;
    const std::uint64_t targetTick = stagedState.completedTicks;
    std::vector<PlayerCommand> commands =
        stagedCommands.takeForTick(targetTick);
    stagedEconomy.resetTickMetrics();
    for (const PlayerCommand& command : commands) {
        std::visit(
            [&](const auto& value) {
                using Command = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Command, SelectChainCommand>) {
                    result.chainSelections.push_back({
                        .command = value,
                        .failure =
                            choosingSkill ? SelectChainFailure::WrongPhase : applySelectChain(value, stagedState, stagedEconomy),
                    });
                } else if constexpr (std::is_same_v<Command, CampaignCommand>) {
                    result.campaignActions.push_back({value, choosingSkill && value.action!=CampaignAction::ChooseSkill
                        ? CampaignFailure::ChooseUpgradeFirst : applyCampaignCommand(stagedWorld, value)});
                } else {
                    if(choosingSkill) {
                        result.placements.push_back({value,PlacementFailure::WrongPhase});
                        return;
                    }
                    if (content_.campaign) {
                        result.placements.push_back(commitPlacementAtomically(value, stagedState,
                            stagedFacilities, stagedStores, stagedEconomy, stagedIdentities.stores()));
                        return;
                    }
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
    if (content_.campaign) { applySkillStoreModifiers(stagedWorld); }
    if (!choosingSkill && content_.phase1 && isPlayingPhase(stagedState.phase)) {
        const auto& rules = *content_.phase1;
        for (std::size_t index = 0; index < kFirstPlayableChainCount; ++index) {
            const auto chain = static_cast<ChainId>(index);
            if (chain == *stagedState.playerChain ||
                matchClock(stagedState) < stagedState.nextAiTick[index]) { continue; }
            const auto command = chooseOpponentPlacement(chain, stagedState, stagedFacilities,
                stagedStores, stagedPopulationCells, stagedEconomy, rules);
            stagedState.nextAiTick[index] = matchClock(stagedState) +
                opponentPeriodTicks(matchClock(stagedState), rules);
            if (command) {
                result.placements.push_back(commitPlacementAtomically(*command, stagedState,
                    stagedFacilities, stagedStores, stagedEconomy, stagedIdentities.stores()));
            }
        }
        for (const auto& placement : result.placements) {
            if (placement.failure != PlacementFailure::None) { continue; }
            stagedState.hasOpened[chainIndex(placement.command.chain)] = true;
            // An intact facility loses population when first replaced. A
            // destroyed lot already paid that loss during destruction and
            // must not lose it a second time when rebuilt.
            if (placement.command.verticalSlot == 0 && placement.replacedIntactFacility) {
                applyPopulationLoss(stagedPopulationCells,
                                    placement.command.facilityId,
                                    rules.destructionPopulationLossPercent);
            }
        }
        ++stagedState.phaseTicks;
        if (content_.campaign) {
            ++stagedState.campaign.elapsedTicks;
            advanceAionInvasion(stagedWorld);
            advanceSkillPulses(stagedWorld);
        }
        stagedTriangles = buildDominantTriangles(stagedStores, rules);
        stagedEncirclements = resolveEncirclement(stagedState, stagedStores, stagedFacilities,
            stagedEconomy, stagedPopulationCells, stagedTriangles, stagedEncirclements, rules);
        if (content_.campaign) {
            resolveAnnihilations(stagedWorld);
            resolveForeignConquest(stagedWorld);
        }
        stagedTriangles = buildDominantTriangles(stagedStores, rules);
        if (content_.campaign) {
            resolveAionMaxValue(stagedWorld, stagedTriangles);
            stagedTriangles = buildDominantTriangles(stagedStores, rules);
        }
        std::erase_if(stagedEncirclements, [&](const auto& threat) {
            const auto target=stagedStores.find(threat.target);
            if(!target || !stagedStores.row(*target).isActive) return true;
            return std::ranges::none_of(stagedTriangles, [&](const auto& triangle) {
                return triangle.chain == threat.attacker && triangle.stores == threat.triangleStores;
            });
        });
        applyTriangleRevenue(stagedStores, stagedTriangles, rules);
        if (content_.campaign) {
            applyConvenienceRevenue(stagedWorld);
            applySkillRevenue(stagedWorld);
        }
        if (matchClock(stagedState) % content_.simulation.economyPeriodTicks == 0) {
            if (content_.campaign) { updateCampaignFaith(stagedWorld, stagedTriangles); }
            // Determine recovery from current ownership, including campaign skills.
            assignCompetitiveCustomers(stagedStores, stagedPopulationCells,
                stagedEconomy, stagedTriangles, rules, verticalUnlocked(stagedState),
                content_.campaign ? skillInfluence(stagedState, content_.campaign->skills)
                                  : SkillInfluence{});
            recoverServedPopulation(stagedPopulationCells, stagedStores, rules);
        }
    }
    std::sort(
        result.placements.begin(),
        result.placements.end(),
        [](const PlacementResult& left, const PlacementResult& right) {
            return commandLess(PlayerCommand(left.command),
                               PlayerCommand(right.command));
        });

    if (content_.phase1) {
        assignCompetitiveCustomers(stagedStores, stagedPopulationCells, stagedEconomy,
                                   stagedTriangles, *content_.phase1, verticalUnlocked(stagedState),
                                   content_.campaign ? skillInfluence(stagedState,content_.campaign->skills) : SkillInfluence{});
    } else {
        (void)assignNearestStores(stagedStores, stagedPopulationCells, stagedEconomy);
    }
    ++stagedState.completedTicks;
    result.economyCollected = !choosingSkill && collectEconomyForCompletedTick(
        content_.phase1 ? matchClock(stagedState) : stagedState.completedTicks,
        content_.simulation.economyPeriodTicks,
        stagedStores,
        stagedEconomy);
    if (!choosingSkill && content_.phase1) {
        if (content_.campaign) {
            resolveCampaignProgression(stagedWorld);
            if(result.economyCollected) awardSkillExperience(stagedWorld);
        }
        else { resolvePhase1Outcome(stagedState, stagedEconomy, stagedPopulationCells, *content_.phase1); }
        if (stagedState.phase == GamePhase::Result) { stagedCommands = CommandQueue{}; }
    }
    result.completedTicks = stagedState.completedTicks;
    result.canonical = makeCanonicalSnapshot(stagedState,
                                             content_,
                                             stagedFacilities,
                                             stagedStores,
                                             stagedPopulationCells,
                                             stagedEconomy, stagedEncirclements);
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
                                       placementCues, stagedTriangles, stagedEncirclements);

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
    triangles_ = std::move(stagedTriangles);
    encirclements_ = std::move(stagedEncirclements);
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
