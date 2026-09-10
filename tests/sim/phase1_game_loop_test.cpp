#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>
#include "konbini/sim/competitive_zoc_system.h"
#include "konbini/sim/encirclement_system.h"
#include "konbini/sim/first_playable_simulation.h"
#include "konbini/sim/opponent_ai_system.h"
#include "konbini/sim/phase1_outcome_system.h"
#include "konbini/sim/aion_system.h"
#include "konbini/sim/population_change_system.h"
#include "konbini/render/facility_display_bounds.h"
#include "konbini/render/facility_picker.h"
#include "konbini/render/phase1_overlay_geometry.h"
#include "konbini/render/zoc_overlay_geometry.h"
#include "konbini/app/hud_text_model.h"

namespace {
using namespace konbini::sim;
void require(const bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}
FirstPlayableContent profile() {
    auto content = loadFirstPlayableContent(KONBINI_PHASE1_CONTENT_FILE);
    content.phase1->durationTicks = 100;
    content.phase1->captureDelayTicks = 2;
    content.phase1->aiOpeningPeriodTicks = 6;
    content.phase1->aiPeriodTicks = 2;
    content.phase1->aiRampTicks = 80;
    return content;
}
StoreRow store(const std::uint32_t id, const ChainId chain, const double x, const double z) {
    StoreRow row;
    row.id = {{id, 1}};
    row.facilityId = {{id, 1}};
    row.chain = chain;
    row.positionMeters = {x, 0, z};
    row.zocRadiusMeters = 30;
    return row;
}
void testTriangles() {
    const auto content = profile();
    StoreTable stores;
    const std::array<StoreRow, 4> rows{
        store(0, ChainId::Losan, 0, 0), store(1, ChainId::Losan, 20, 0),
        store(2, ChainId::Losan, 20, 20), store(3, ChainId::Losan, 0, 20)};
    for (const auto& row : rows) { stores.append(row); }
    const auto triangles = buildDominantTriangles(stores, *content.phase1);
    require(triangles.size() == 2, "cocircular square must have one deterministic diagonal");
    StoreTable reversed;
    for (auto i = rows.rbegin(); i != rows.rend(); ++i) { reversed.append(*i); }
    const auto reordered = buildDominantTriangles(reversed, *content.phase1);
    require(reordered.size() == triangles.size(), "triangle count depends on dense order");
    for (std::size_t i = 0; i < triangles.size(); ++i) {
        require(reordered[i].stores == triangles[i].stores, "triangle IDs depend on dense order");
    }
    require(triangleContains(triangles.front(), triangles.front().points.front()), "triangle boundary excluded");
    require(!triangleContains(DominantTriangle{}, {}), "degenerate triangle contains a point");
    require(stores.deactivate({{1, 1}}), "could not remove triangle vertex");
    const auto remaining = buildDominantTriangles(stores, *content.phase1);
    require(remaining.size() == 1, "deleted vertex did not rebuild triangles");
}
void testInfluenceAndIncome() {
    const auto content = profile();
    StoreTable stores;
    stores.append(store(0, ChainId::Losan, 0, 0));
    stores.append(store(1, ChainId::Famoma, 2, 0));
    PopulationCellTable cells;
    PopulationCellRow cell;
    cell.id = {{0, 1}};
    cell.facilityId = {{0, 1}};
    cell.positionMeters = {2, 0, 0};
    cell.population = 100;
    cell.assignedStore = StoreId{{0, 1}};
    cell.preferredChain = ChainId::Losan;
    cells.append(cell);
    ChainEconomyTable economy(content);
    assignCompetitiveCustomers(stores, cells, economy, {}, *content.phase1);
    require(cells.row(0).preferredChain == ChainId::Losan, "equal influence must keep current owner");
    stores.append(store(2, ChainId::Famoma, 3, 0));
    assignCompetitiveCustomers(stores, cells, economy, {}, *content.phase1);
    require(cells.row(0).preferredChain == ChainId::Famoma, "greater store influence did not take cell");
    require(economy.row(ChainId::Losan).customerShare == 0 &&
            economy.row(ChainId::Famoma).customerShare == 100, "population was counted twice");
}
void testRecoveryUsesCurrentOwnership() {
    const auto content = profile();
    StoreTable stores;
    stores.append(store(0, ChainId::Losan, 0, 0));
    PopulationCellTable cells;
    PopulationCellRow cell;
    cell.id = {{0, 1}};
    cell.facilityId = {{0, 1}};
    cell.positionMeters = {100, 0, 0};
    cell.population = 50;
    cell.capacity = 100;
    cell.assignedStore = StoreId{{0, 1}};
    cell.preferredChain = ChainId::Losan;
    cells.append(cell);
    ChainEconomyTable economy(content);
    assignCompetitiveCustomers(stores, cells, economy, {}, *content.phase1);
    recoverServedPopulation(cells, stores, *content.phase1);
    require(cells.row(0).population == 50,
            "stale ownership recovered an unserved population cell");
}
void testRebuildDoesNotReplaceAnIntactFacility() {
    const auto content = profile();
    GameState state;
    state.competitive = true;
    state.phase = GamePhase::Phase1;
    state.playerChain = ChainId::Losan;
    FacilityTable facilities;
    FacilityRow facility;
    facility.id = {{0, 1}};
    facility.figmentumKey = {1};
    facility.positionMeters = {};
    facility.boundsMeters = {{-1, 0, -1}, {1, 1, 1}};
    facility.isBuildable = true;
    facilities.append(facility);
    StoreTable stores;
    ChainEconomyTable economy(content);
    require(economy.activate(ChainId::Losan), "player activation failed");
    GenerationalIdPool<StoreId> storeIds;
    const PlaceStoreCommand command{{0, CommandSourcePriority::Player, 0},
                                    ChainId::Losan, facility.id, 0};
    const auto initial = commitPlacementAtomically(
        command, state, facilities, stores, economy, storeIds);
    require(initial.placedStore.has_value() && initial.replacedIntactFacility,
            "initial placement lost replacement provenance");
    require(stores.deactivate(*initial.placedStore), "could not prepare destroyed lot");
    require(facilities.setState(facility.id, FacilityState::Destroyed),
            "could not prepare destroyed facility");
    economy.removeStore(ChainId::Losan);
    const auto rebuilt = commitPlacementAtomically(
        command, state, facilities, stores, economy, storeIds);
    require(rebuilt.failure == PlacementFailure::None &&
            !rebuilt.replacedIntactFacility,
            "rebuild was reported as another intact-facility replacement");
}
void testEncirclement() {
    const auto content = profile();
    GameState state;
    state.competitive = true;
    state.phase = GamePhase::Phase1;
    state.playerChain = ChainId::Losan;
    StoreTable stores;
    FacilityTable facilities;
    ChainEconomyTable economy(content);
    require(economy.activate(ChainId::Losan) && economy.activate(ChainId::Famoma), "activation failed");
    const std::array<StoreRow, 4> rows{
        store(0, ChainId::Losan, 0, 0), store(1, ChainId::Losan, 20, 0),
        store(2, ChainId::Losan, 0, 20), store(3, ChainId::Famoma, 5, 5)};
    for (const auto& row : rows) {
        stores.append(row);
        FacilityRow facility;
        facility.id = row.facilityId;
        facility.figmentumKey = {row.id.value().index + 1ULL};
        facility.positionMeters = row.positionMeters;
        facility.boundsMeters = {{row.positionMeters.x-1, 0, row.positionMeters.z-1},
                                  {row.positionMeters.x+1, 5, row.positionMeters.z+1}};
        facility.isBuildable = true;
        facility.state = FacilityState::Replaced;
        facilities.append(facility);
        require(economy.debitPlacement(row.chain, economy.rules(row.chain).buildCostCredits), "budget fixture failed");
    }
    PopulationCellTable cells;
    const auto triangles = buildDominantTriangles(stores, *content.phase1);
    auto warnings = resolveEncirclement(state, stores, facilities, economy, cells, triangles, {}, *content.phase1);
    require(warnings.size() == 1 && warnings[0].elapsedTicks == 1,
            "detection tick must count toward the capture delay");
    auto brokenStores = stores;
    require(brokenStores.deactivate({{0, 1}}), "could not break attacker triangle");
    auto brokenFacilities = facilities;
    auto brokenEconomy = economy;
    auto brokenState = state;
    const auto canceled = resolveEncirclement(brokenState, brokenStores, brokenFacilities, brokenEconomy,
        cells, buildDominantTriangles(brokenStores, *content.phase1), warnings, *content.phase1);
    require(canceled.empty() && brokenStores.row(3).isActive, "broken triangle failed to cancel threat");
    warnings = resolveEncirclement(state, stores, facilities, economy, cells, triangles, warnings, *content.phase1);
    require(!stores.row(3).isActive && warnings.empty(), "capture failed to remove enemy");
    require(economy.row(ChainId::Famoma).storeCount == 0 && state.destroyedStores[0] == 1,
            "capture statistics not committed");
    PlaceStoreCommand rebuild{{0, CommandSourcePriority::Player, 0}, ChainId::Losan, {{3, 1}}, 0};
    require(validatePlacement(rebuild, state, facilities, stores, economy).failure == PlacementFailure::None,
            "destroyed lot is not rebuildable");
}
void testOutcomes() {
    const auto content = profile();
    GameState state;
    state.phase = GamePhase::Phase1;
    state.competitive = true;
    state.playerChain = ChainId::Losan;
    state.hasOpened.fill(true);
    ChainEconomyTable economy(content);
    require(economy.activate(ChainId::Losan), "player activation failed");
    require(economy.debitPlacement(ChainId::Losan, economy.rules(ChainId::Losan).buildCostCredits), "player store failed");
    PopulationCellTable cells;
    PopulationCellRow cell;
    cell.id = {{0, 1}};
    cell.facilityId = {{0, 1}};
    cell.population = 100;
    cells.append(cell);
    economy.setCustomerShare(ChainId::Losan, 60);
    resolvePhase1Outcome(state, economy, cells, *content.phase1);
    require(state.outcome == MatchOutcome::Win && state.endReason == MatchEndReason::Domination,
            "60 percent domination should win");
    state.phase = GamePhase::Phase1;
    state.phaseTicks = content.phase1->durationTicks;
    state.hasOpened.fill(false);
    economy.setCustomerShare(ChainId::Losan, 50);
    economy.setCustomerShare(ChainId::Famoma, 50);
    resolvePhase1Outcome(state, economy, cells, *content.phase1);
    require(state.outcome == MatchOutcome::Draw && state.endReason == MatchEndReason::TimeLimit,
            "tied leaders should draw at timeout");
    state.phase = GamePhase::Phase1;
    economy.setCustomerShare(ChainId::Famoma, 51);
    resolvePhase1Outcome(state, economy, cells, *content.phase1);
    require(state.outcome == MatchOutcome::Lose, "larger rival share should win timeout");
}
FirstPlayableSimulation match(const FirstPlayableContent& content) {
    WorldEntityIds ids;
    FacilityTable facilities;
    for (std::uint32_t i = 0; i < 16; ++i) {
        const auto id = ids.facilities().acquire();
        const double x = static_cast<double>(i % 4) * 15.0;
        const double z = static_cast<double>(i / 4) * 15.0;
        facilities.append({id, {i + 1ULL}, 0, {x, 0, z}, {{x-2, 0, z-2}, {x+2, 5, z+2}}, true});
    }
    return FirstPlayableSimulation(content, facilities, ids);
}
void testMatchEndAndDeterminism() {
    const auto content = profile();
    auto a = match(content), b = match(content);
    const SelectChainCommand select{{0, CommandSourcePriority::Player, 0}, ChainId::Losan};
    a.submit(select);
    b.submit(select);
    for (unsigned tick = 0; tick < content.phase1->durationTicks; ++tick) {
        const auto left = a.completeNextTick(), right = b.completeNextTick();
        require(left.canonical.schemaVersion == kPhase1CanonicalSnapshotSchemaVersion,
                "phase1 snapshot did not use the current schema");
        require(left.canonical.hash == right.canonical.hash, "same command stream diverged");
    }
    require(a.state().phase == GamePhase::Result, "match did not end at timeout");
    const auto frozen = a.completeNextTick();
    const auto again = a.completeNextTick();
    require(frozen.canonical.hash == again.canonical.hash && frozen.completedTicks == again.completedTicks,
            "result did not freeze simulation");
    bool rejected = false;
    try { a.submit(SelectChainCommand{{a.state().completedTicks, CommandSourcePriority::Player, 1}, ChainId::Famoma}); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "result accepted a gameplay command");
    auto fresh = match(content);
    require(fresh.state().phase == GamePhase::ChainSelect && fresh.stores().size() == 0 &&
            fresh.state().phaseTicks == 0, "fresh match inherited old state");
}
void testPresentationStateDoesNotChangeCanonicalState() {
    const auto content = loadFirstPlayableContent(KONBINI_CAMPAIGN_CONTENT_FILE);
    GameState origin;
    origin.campaign.enabled = true;
    origin.campaign.visibleDimension = 0;
    auto foreignView = origin;
    foreignView.campaign.visibleDimension = 7;
    const FacilityTable facilities;
    const StoreTable stores;
    const PopulationCellTable population;
    const ChainEconomyTable economy(content);
    const auto originSnapshot = makeCanonicalSnapshot(
        origin, content, facilities, stores, population, economy);
    const auto foreignSnapshot = makeCanonicalSnapshot(
        foreignView, content, facilities, stores, population, economy);
    require(originSnapshot.schemaVersion == kCanonicalSnapshotSchemaVersion,
            "campaign snapshot does not advertise the current schema");
    require(originSnapshot.bytes == foreignSnapshot.bytes &&
            originSnapshot.hash == foreignSnapshot.hash,
            "view-only dimension navigation changed canonical state");
}
void testCampaignAiUsesContentSlotLimit() {
    const auto content = loadFirstPlayableContent(KONBINI_CAMPAIGN_CONTENT_FILE);
    GameState state;
    state.phase = GamePhase::Phase2;
    state.playerChain = ChainId::Losan;
    state.competitive = true;
    state.campaign.enabled = true;
    state.campaign.dimensions.push_back({0, state.worldSeed});
    FacilityTable facilities;
    FacilityRow facility;
    facility.id = {{50, 1}};
    facility.figmentumKey = {50};
    facility.positionMeters = {0, 0, 0};
    facility.boundsMeters = {{-2, 0, -2}, {2, 5, 2}};
    facility.isBuildable = true;
    facility.state = FacilityState::Replaced;
    facilities.append(facility);
    StoreTable stores;
    for (std::uint32_t slot = 0; slot < 8; ++slot) {
        auto row = store(slot, ChainId::Famoma, 0, 0);
        row.facilityId = facility.id;
        row.verticalSlot = slot;
        stores.append(row);
    }
    PopulationCellTable population;
    ChainEconomyTable economy(content);
    require(economy.activate(ChainId::Famoma), "AI economy activation failed");
    const auto command = chooseOpponentPlacement(
        ChainId::Famoma, state, facilities, stores, population, economy,
        *content.phase1);
    require(command.has_value() && command->verticalSlot == 8,
            "campaign AI retained a hard-coded eight-floor limit");
}
void testDestroyedLotPicking() {
    konbini::sim::RenderFacility facility;
    facility.id = {{0, 1}};
    facility.figmentumKey = {1};
    facility.boundsMeters = {{-5, 0, -5}, {5, 40, 5}};
    facility.isBuildable = true;
    facility.state = FacilityState::Destroyed;
    facility.isLotRepresentation = true;
    const std::array facilities{facility};
    const konbini::render::WorldRay ray{{0, 20, 0}, {0, -1, 0}};
    require(konbini::render::pickFacility(ray, facilities).has_value(), "destroyed lot cannot be selected");
    const auto bounds = konbini::render::facilityDisplayBounds(facility);
    require(bounds.max.y < 1, "destroyed lot retained invisible building hitbox");
}
// @implements spec/feature/full-campaign-baseline.md Vertical presentation
// @implements spec/test/verification-strategy.md 1. Data / ID unit tests
void testHighFloorOverlays() {
    const auto content = profile();
    GameState state;
    state.phase = GamePhase::Phase1;
    state.competitive = true;
    state.playerChain = ChainId::Losan;
    StoreTable stores;
    auto elevated = store(0, ChainId::Losan, 0, 0);
    elevated.positionMeters.y = 320;
    stores.append(elevated);
    FacilityTable facilities;
    PopulationCellTable cells;
    ChainEconomyTable economy(content);
    const DominantTriangle triangle{
        {StoreId{{0, 1}}, StoreId{{1, 1}}, StoreId{{2, 1}}},
        {Vec3{0, 320, 0}, Vec3{20, 320, 0}, Vec3{0, 320, 20}},
        ChainId::Losan, 0};
    const Encirclement threat{elevated.id, ChainId::Famoma, {}, 1};
    const auto snapshot = makeRenderSnapshot(state, content, facilities, stores,
        cells, economy, {}, std::span(&triangle, 1), std::span(&threat, 1));
    const auto phaseOverlay = konbini::render::buildPhase1OverlayGeometry(*snapshot);
    require(!phaseOverlay.vertices.empty() &&
            std::ranges::all_of(phaseOverlay.vertices, [](const auto& vertex) {
                return vertex.position[1] > 300;
            }), "phase overlay discarded the selected floor height");
    const auto zoc = konbini::render::buildZocOverlayGeometry(snapshot->stores(), 8, 0.03F);
    require(!zoc.vertices.empty() &&
            std::ranges::all_of(zoc.vertices, [](const auto& vertex) {
                return vertex.position[1] > 300;
            }), "ZOC overlay discarded the store floor height");
}
// @implements spec/feature/full-campaign-baseline.md Aion
// @implements spec/test/verification-strategy.md 2. Deterministic simulation
void testAionPasteLimitIsGlobal() {
    auto content = loadFirstPlayableContent(KONBINI_CAMPAIGN_CONTENT_FILE);
    GameState state;
    state.phase = GamePhase::Boss;
    state.playerChain = ChainId::Losan;
    state.competitive = true;
    state.campaign.enabled = true;
    state.campaign.elapsedTicks = content.campaign->aion.historyTicks;
    state.campaign.nextBossSpawnTick = std::numeric_limits<std::uint64_t>::max();
    state.campaign.dimensions = {{0, 1}, {1, 2}};
    FacilityTable facilities;
    StoreTable stores;
    PopulationCellTable population;
    ChainEconomyTable economy(content);
    WorldEntityIds identities;
    AionHistoryFrame history;
    history.tick = 0;
    for (std::uint32_t dimension = 0; dimension < 2; ++dimension) {
        for (std::uint32_t lot = 0; lot < 6; ++lot) {
            const auto id = identities.facilities().acquire();
            facilities.append({id, {lot + 1ULL}, dimension,
                {static_cast<double>(lot) * 10, 0, static_cast<double>(dimension) * 10},
                {{static_cast<double>(lot) * 10 - 1, 0, static_cast<double>(dimension) * 10 - 1},
                 {static_cast<double>(lot) * 10 + 1, 5, static_cast<double>(dimension) * 10 + 1}},
                true});
            if (lot < content.campaign->aion.pasteLimit) {
                history.stores.push_back({dimension, 0, lot + 1ULL});
            }
        }
    }
    state.campaign.history.push_back(std::move(history));
    require(economy.activate(ChainId::Aion), "Aion activation failed");
    economy.creditRevenue(ChainId::Aion, content.campaign->aion.budgetCredits);
    CampaignWorld world{state, content, facilities, stores, population, economy, identities};
    advanceAionInvasion(world);
    require(economy.row(ChainId::Aion).storeCount == content.campaign->aion.pasteLimit,
            "one time-paste event exceeded its global store limit");
}
}
int main() {
    try {
        testTriangles();
        testInfluenceAndIncome();
        testRecoveryUsesCurrentOwnership();
        testRebuildDoesNotReplaceAnIntactFacility();
        testEncirclement();
        testOutcomes();
        testMatchEndAndDeterminism();
        testPresentationStateDoesNotChangeCanonicalState();
        testCampaignAiUsesContentSlotLimit();
        testDestroyedLotPicking();
        testHighFloorOverlays();
        testAionPasteLimitIsGlobal();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
