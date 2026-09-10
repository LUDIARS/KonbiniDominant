#include "konbini/sim/placement_system.h"

#include <stdexcept>
#include <utility>
#include "konbini/sim/vertical_placement.h"
#include "konbini/sim/skill_system.h"

// @implements spec/feature/phase-1-dominant-triangle.md 店舗配置
// @implements spec/feature/economy-and-population.md 建設費

namespace konbini::sim {

// REQ-P1-PLACE の成立条件を1か所で判定する。失敗は理由付きの値で返し、
// world state と cash を一切変更しない (world-state.md#Commands)。
// @implements spec/feature/phase-1-dominant-triangle.md 店舗配置
PlacementDecision validatePlacement(const PlaceStoreCommand& command,
                                    const GameState& state,
                                    const FacilityTable& facilities,
                                    const StoreTable& stores,
                                    const ChainEconomyTable& economy) {
    if (state.campaign.skills.pending || !isPlayingPhase(state.phase) || (state.phase != GamePhase::Phase1 && !state.campaign.enabled)) {
        return {.failure = PlacementFailure::WrongPhase};
    }
    if (!state.playerChain.has_value()) {
        return {.failure = PlacementFailure::NoPlayerChain};
    }
    if (!isSimulationChainId(command.chain) ||
        (command.chain == ChainId::Aion && (!state.campaign.enabled || state.phase != GamePhase::Boss))) {
        return {.failure = PlacementFailure::InvalidChain};
    }
    const bool isPlayer = command.order.sourcePriority == CommandSourcePriority::Player;
    const bool isAi = command.order.sourcePriority == CommandSourcePriority::Ai;
    if ((!isPlayer && !isAi) ||
        (isPlayer && *state.playerChain != command.chain) ||
        (isAi && (!state.competitive || *state.playerChain == command.chain))) {
        return {.failure = PlacementFailure::WrongChain};
    }
    if (command.verticalSlot != 0 &&
        (!verticalUnlocked(state) || !economy.campaignRules() ||
         command.verticalSlot >= economy.campaignRules()->vertical.slots)) {
        return {.failure = PlacementFailure::UnsupportedVerticalSlot};
    }

    const std::optional<std::size_t> facilityIndex =
        facilities.find(command.facilityId);
    if (!facilityIndex.has_value()) {
        return {.failure = PlacementFailure::FacilityNotFound};
    }
    const FacilityRow facility = facilities.row(*facilityIndex);
    if (state.campaign.enabled && !dimensionActive(state.campaign, facility.dimension)) {
        return {.failure = PlacementFailure::DimensionCollapsed};
    }
    if (!facility.isBuildable) {
        return {.failure = PlacementFailure::FacilityProtected};
    }
    if (!verticalUnlocked(state) && facility.state != FacilityState::Intact &&
        !(state.competitive && facility.state == FacilityState::Destroyed)) {
        return {.failure = PlacementFailure::FacilityUnavailable};
    }
    if (stores.findAt(command.facilityId, command.verticalSlot)) {
        return {.failure = PlacementFailure::FacilityOccupied};
    }

    if (command.verticalSlot > 0 && !stores.findAt(command.facilityId, command.verticalSlot - 1)) {
        return {.failure = PlacementFailure::MissingSupport};
    }
    const ChainContent& rules = economy.rules(command.chain);
    auto cost = economy.campaignRules()
        ? verticalBuildCost(rules.buildCostCredits, command.verticalSlot, economy.campaignRules()->vertical)
        : rules.buildCostCredits;
    if (economy.campaignRules())
        cost=skillBuildCost(cost,state,economy.campaignRules()->skills,command.chain);
    if (!economy.canAfford(command.chain, cost)) {
        return {.failure = PlacementFailure::InsufficientCash};
    }
    return {
        .failure = PlacementFailure::None,
        .buildCostCredits = cost,
        .zocRadiusMeters = rules.zocRadiusMeters,
    };
}

// facility 破壊と store spawn は同じ structural transaction として適用し、
// 片方だけ成功する中間状態を作らない
// (phase-1-dominant-triangle.md#店舗配置)。
// @implements spec/feature/phase-1-dominant-triangle.md 店舗配置
// @implements spec/data/world-state.md Structural commands
PlacementResult commitPlacementAtomically(
    const PlaceStoreCommand& command, const GameState& state,
    FacilityTable& facilities, StoreTable& stores,
    ChainEconomyTable& economy, GenerationalIdPool<StoreId>& storeIds) {
    const PlacementDecision decision =
        validatePlacement(command, state, facilities, stores, economy);
    if (decision.failure != PlacementFailure::None) {
        return {.command = command, .failure = decision.failure};
    }

    // Structural mutation is prepared against value copies. The authoritative
    // tables are replaced only after the facility replacement, cash debit and
    // store append have all succeeded. The copies start out identical to the
    // validated tables, so `decision` still describes them.
    FacilityTable stagedFacilities = facilities;
    StoreTable stagedStores = stores;
    ChainEconomyTable stagedEconomy = economy;
    GenerationalIdPool<StoreId> stagedStoreIds = storeIds;

    const std::size_t facilityIndex =
        *stagedFacilities.find(command.facilityId);
    const FacilityRow facility = stagedFacilities.row(facilityIndex);
    const StoreId storeId = stagedStoreIds.acquire();
    auto position = facility.positionMeters;
    if (economy.campaignRules()) { position.y += command.verticalSlot * economy.campaignRules()->vertical.floorHeightMeters; }
    stagedStores.append({
        .id = storeId,
        .facilityId = command.facilityId,
        .chain = command.chain,
        .dimension = facility.dimension,
        .positionMeters = position,
        .zocRadiusMeters = decision.zocRadiusMeters,
        .capturedPopulation = 0,
        .isActive = true,
        .verticalSlot = command.verticalSlot,
        .faith = economy.campaignRules() ? economy.campaignRules()->vertical.startingFaith : 0,
    });
    if (!stagedFacilities.setState(command.facilityId,
                                   FacilityState::Replaced)) {
        throw std::logic_error(
            "validated facility disappeared during placement commit");
    }
    if (!stagedEconomy.debitPlacement(command.chain,
                                      decision.buildCostCredits)) {
        throw std::logic_error(
            "validated chain cash changed during placement commit");
    }

    facilities = std::move(stagedFacilities);
    stores = std::move(stagedStores);
    economy = std::move(stagedEconomy);
    storeIds = std::move(stagedStoreIds);
    return {
        .command = command,
        .failure = PlacementFailure::None,
        .placedStore = storeId,
        .replacedIntactFacility = facility.state == FacilityState::Intact,
    };
}

}  // namespace konbini::sim
