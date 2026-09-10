#include "konbini/sim/render_snapshot.h"

#include <limits>
#include <algorithm>
#include <stdexcept>
#include <utility>

#include "konbini/sim/revenue_math.h"
#include "konbini/sim/opponent_ai_system.h"
#include "konbini/sim/vertical_placement.h"
#include "konbini/sim/skill_system.h"

// @implements spec/interface/pictor-rendering.md `RenderSnapshot`
// @implements spec/feature/ui-ux.md Common HUD
// @implements spec/feature/npc-conversations-and-placement-feedback.md Determinism and ownership
// @implements spec/interface/visia-presentation.md Pictor integration boundary

namespace konbini::sim {

// @implements spec/interface/pictor-rendering.md `RenderSnapshot`
std::uint64_t RenderSnapshot::completedTicks() const noexcept {
    return completedTicks_;
}

// @implements spec/interface/pictor-rendering.md `RenderSnapshot`
std::span<const RenderFacility> RenderSnapshot::facilities() const noexcept {
    return facilities_;
}

// @implements spec/interface/pictor-rendering.md `RenderSnapshot`
std::span<const RenderStore> RenderSnapshot::stores() const noexcept {
    return stores_;
}

// @implements spec/feature/npc-conversations-and-placement-feedback.md Ambient resident baseline
std::span<const ResidentPresentation> RenderSnapshot::residents() const noexcept {
    return residents_;
}

// @implements spec/feature/npc-conversations-and-placement-feedback.md Store placement choreography
std::span<const RenderStorePlacementCue>
RenderSnapshot::placementCues() const noexcept {
    return placementCues_;
}

// @implements spec/feature/ui-ux.md Common HUD
std::span<const DominantTriangle> RenderSnapshot::triangles() const noexcept {
    return triangles_;
}
std::span<const RenderPopulationCell> RenderSnapshot::populationCells() const noexcept {
    return populationCells_;
}
const HudViewModel& RenderSnapshot::hud() const noexcept {
    return hud_;
}

// tick 終端で publish する immutable snapshot を組み立てる。render thread は
// これを読むだけなので、simulation table への参照は一切残さず値で写し取る。
// @implements spec/interface/pictor-rendering.md `RenderSnapshot`
std::shared_ptr<const RenderSnapshot> makeRenderSnapshot(
    const GameState& state, const FirstPlayableContent& content,
    const FacilityTable& facilities, const StoreTable& stores,
    const PopulationCellTable& populationCells,
    const ChainEconomyTable& economy,
    const std::span<const RenderStorePlacementCue> placementCues,
    const std::span<const DominantTriangle> triangles,
    const std::span<const Encirclement> encirclements) {
    if (content.simulation.economyPeriodTicks == 0) {
        throw std::invalid_argument("economy period must be positive");
    }
    auto snapshot = std::make_shared<RenderSnapshot>();
    snapshot->completedTicks_ = state.completedTicks;
    snapshot->facilities_.reserve(facilities.size());
    for (std::size_t index = 0; index < facilities.size(); ++index) {
        const FacilityRow row = facilities.row(index);
        if (state.campaign.enabled && (row.dimension != state.campaign.visibleDimension ||
            !dimensionActive(state.campaign, row.dimension))) { continue; }
        snapshot->facilities_.push_back({
            .id = row.id,
            .figmentumKey = row.figmentumKey,
            .positionMeters = row.positionMeters,
            .boundsMeters = row.boundsMeters,
            .state = row.state,
            .isBuildable = row.isBuildable,
            .isLotRepresentation = state.competitive && row.state != FacilityState::Intact,
            .allowsStacking = verticalUnlocked(state),
        });
    }
    snapshot->stores_.reserve(stores.size());
    for (std::size_t index = 0; index < stores.size(); ++index) {
        const StoreRow row = stores.row(index);
        if (!row.isActive || (state.campaign.enabled &&
            (row.dimension != state.campaign.visibleDimension || !dimensionActive(state.campaign, row.dimension)))) {
            continue;
        }
        snapshot->stores_.push_back({
            .id = row.id,
            .facilityId = row.facilityId,
            .chain = row.chain,
            .positionMeters = row.positionMeters,
            .zocRadiusMeters = row.zocRadiusMeters,
            .capturedPopulation = row.capturedPopulation,
            .revenuePermille = row.revenuePermille,
            .verticalSlot = row.verticalSlot, .faith = row.faith, .isAntiStore = row.isAntiStore,
            .dimension = row.dimension,
        });
    }
    for (const auto& triangle : triangles) {
        if (!state.campaign.enabled || (triangle.dimension == state.campaign.visibleDimension &&
            dimensionActive(state.campaign, triangle.dimension))) { snapshot->triangles_.push_back(triangle); }
    }
    auto& hud = snapshot->hud_;
    hud.competitive = state.competitive;
    hud.phase = state.phase;
    hud.outcome = state.outcome;
    hud.endReason = state.endReason;
    hud.phaseTicks = state.phaseTicks;
    hud.ticksPerSecond = content.simulation.ticksPerSecond;
    for (std::size_t i = 0; i < kFirstPlayableChainCount; ++i) {
        const auto row = economy.row(static_cast<ChainId>(i));
        hud.chains[i] = {row.chain, row.storeCount, row.customerShare, 0,
                         content.campaign ? skillBuildCost(economy.rules(row.chain).buildCostCredits,
                            state,content.campaign->skills,row.chain) : economy.rules(row.chain).buildCostCredits};
    }
    for (const auto& triangle : triangles) {
        if (isFirstPlayableChainId(triangle.chain)) { ++hud.chains[chainIndex(triangle.chain)].triangles; }
    }
    if (content.campaign) { hud.campaign = makeCampaignHud(state, *content.campaign, stores); }
    for (std::size_t i = 0; i < populationCells.size(); ++i) {
        const auto cell = populationCells.row(i);
        if (hud.totalPopulation > std::numeric_limits<std::uint64_t>::max() - cell.population) {
            throw std::overflow_error("HUD total population overflow");
        }
        hud.totalPopulation += cell.population;
        if (!state.campaign.enabled || (cell.dimension == state.campaign.visibleDimension &&
            dimensionActive(state.campaign, cell.dimension))) {
            snapshot->populationCells_.push_back({cell.positionMeters, cell.preferredChain, cell.population});
        }
    }
    if (content.phase1) {
        hud.ticksRemaining = content.phase1->durationTicks -
            std::min<std::uint64_t>(state.phaseTicks, content.phase1->durationTicks);
        hud.dominationPercent = content.phase1->dominationPercent;
        hud.aiPeriodTicks = opponentPeriodTicks(matchClock(state), *content.phase1);
        hud.captureDelayTicks = content.phase1->captureDelayTicks;
        if (state.playerChain) {
            hud.destroyedRivalStores = state.destroyedStores[chainIndex(*state.playerChain)];
        }
        for (auto& store : snapshot->stores_) {
            const auto threat = std::ranges::find_if(encirclements, [&](const auto& value) {
                return value.target == store.id;
            });
            if (threat == encirclements.end()) { continue; }
            store.isEncircled = true;
            store.captureDelayTotal = content.campaign ? skillCaptureDelay(content.phase1->captureDelayTicks,
                state,content.campaign->skills,store.chain) : content.phase1->captureDelayTicks;
            store.captureTicksRemaining = store.captureDelayTotal -
                std::min(threat->elapsedTicks, store.captureDelayTotal);
            if (state.playerChain && store.chain == *state.playerChain) { ++hud.threatenedPlayerStores; }
        }
    }
    PopulationCellTable visiblePopulation;
    if (state.campaign.enabled) {
        for (std::size_t i=0; i<populationCells.size(); ++i) {
            const auto cell=populationCells.row(i);
            if (cell.dimension==state.campaign.visibleDimension && dimensionActive(state.campaign,cell.dimension)) {
                visiblePopulation.append(cell);
            }
        }
    }
    snapshot->residents_ = projectResidentPresentations(
        state.completedTicks,
        state.worldSeed,
        content.simulation.ticksPerSecond,
        content.residentPresentation,
        state.campaign.enabled ? visiblePopulation : populationCells,
        stores);
    for (const auto& cue : placementCues) {
        const auto index=stores.find(cue.storeId);
        if (index && stores.row(*index).isActive && (!state.campaign.enabled ||
            stores.row(*index).dimension==state.campaign.visibleDimension)) snapshot->placementCues_.push_back(cue);
    }
    snapshot->hud_.completedTicks = state.completedTicks;
    snapshot->hud_.playerChain = state.playerChain;
    const std::uint32_t remainder = static_cast<std::uint32_t>(
        (content.phase1 ? matchClock(state) : state.completedTicks) % content.simulation.economyPeriodTicks);
    snapshot->hud_.ticksUntilEconomy =
        remainder == 0 ? content.simulation.economyPeriodTicks
                       : content.simulation.economyPeriodTicks - remainder;
    if (state.playerChain.has_value()) {
        const ChainEconomyRow row = economy.row(*state.playerChain);
        snapshot->hud_.cashCredits = row.cashCredits;
        snapshot->hud_.storeCount = row.storeCount;
        // 予測値は economy が実際に適用する rule から引く。`content` は同一
        // instance とは限らず、別 content から引くと HUD の予測と tick 決算が
        // 無言で食い違う。
        const ChainContent& rules = economy.rules(*state.playerChain);
        for (std::size_t i=0; i<stores.size(); ++i) {
            const auto store=stores.row(i);
            if (!store.isActive || store.chain != *state.playerChain) {
                continue;
            }
            const std::int64_t revenue = calculateStoreRevenueCredits(
                store.capturedPopulation,
                rules.revenueMilliCreditsPerPerson, store.revenuePermille);
            if (snapshot->hud_.predictedEconomyIncomeCredits >
                std::numeric_limits<std::int64_t>::max() - revenue) {
                throw std::overflow_error(
                    "predicted player economy income overflow");
            }
            snapshot->hud_.predictedEconomyIncomeCredits += revenue;
        }
    }
    return std::shared_ptr<const RenderSnapshot>(std::move(snapshot));
}

}  // namespace konbini::sim
