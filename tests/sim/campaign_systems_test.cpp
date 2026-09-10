#include "../check.h"
#include <cstdint>
#include "konbini/sim/aion_system.h"
#include "konbini/sim/campaign_progression.h"
#include "konbini/sim/first_playable_content.h"
#include "konbini/sim/opponent_ai_system.h"

namespace {
using namespace konbini::sim;

StoreRow stackedStore(const std::uint32_t id, const FacilityId facility,
                      const std::uint32_t slot, const double floorHeight) {
    StoreRow row;
    row.id = {{id, 1}};
    row.facilityId = facility;
    row.chain = ChainId::Famoma;
    row.positionMeters.y = slot * floorHeight;
    row.zocRadiusMeters = 24;
    row.verticalSlot = slot;
    return row;
}

void configuredAionBudgetAndVerticalAi() {
    const auto content = loadFirstPlayableContent(KONBINI_CAMPAIGN_CONTENT_FILE);
    ChainEconomyTable economy(content);
    CHECK(economy.activate(ChainId::Losan));
    GameState state;
    state.phase = GamePhase::BossWarning;
    state.phaseTicks = content.campaign->aion.warningTicks;
    state.playerChain = ChainId::Losan;
    state.competitive = true;
    state.campaign.enabled = true;
    state.campaign.dimensions.push_back({.id = 0});
    FacilityTable facilities;
    StoreTable stores;
    PopulationCellTable cells;
    WorldEntityIds identities;
    resolveCampaignProgression(
        {state, content, facilities, stores, cells, economy, identities});
    CHECK(economy.row(ChainId::Aion).cashCredits == content.campaign->aion.budgetCredits);

    state.phase = GamePhase::Phase2;
    CHECK(economy.activateWithCash(ChainId::Famoma, 1'000'000));

    FacilityRow facility;
    facility.id = {{0, 1}};
    facility.figmentumKey = {1};
    facility.boundsMeters = {{-6, 0, -6}, {6, 1, 6}};
    facility.isBuildable = true;
    facility.state = FacilityState::Replaced;
    facilities.append(facility);

    for (std::uint32_t slot = 0; slot < 8; ++slot) {
        stores.append(stackedStore(
            slot, facility.id, slot, content.campaign->vertical.floorHeightMeters));
    }
    const auto placement = chooseOpponentPlacement(
        ChainId::Famoma, state, facilities, stores, cells, economy, *content.phase1);
    CHECK(placement && placement->facilityId == facility.id && placement->verticalSlot == 8);
}

void aionGroundInvasionReducesPopulation() {
    const auto content = loadFirstPlayableContent(KONBINI_CAMPAIGN_CONTENT_FILE);
    GameState state;
    state.phase = GamePhase::Boss;
    state.playerChain = ChainId::Losan;
    state.competitive = true;
    state.campaign.enabled = true;
    state.campaign.nextTimePasteTick = 1;
    state.campaign.dimensions.push_back({.id = 0});

    FacilityTable facilities;
    FacilityRow facility;
    facility.id = {{0, 1}};
    facility.figmentumKey = {1};
    facility.boundsMeters = {{-6, 0, -6}, {6, 1, 6}};
    facility.isBuildable = true;
    facilities.append(facility);

    PopulationCellTable population;
    population.append({
        .id = {{0, 1}},
        .facilityId = facility.id,
        .population = 100,
    });
    StoreTable stores;
    ChainEconomyTable economy(content);
    CHECK(economy.activateWithCash(
        ChainId::Aion, content.campaign->aion.budgetCredits));
    WorldEntityIds identities;
    CampaignWorld world{
        state, content, facilities, stores, population, economy, identities};

    advanceAionInvasion(world);

    const auto expectedPopulation = 100U -
        100U * content.phase1->destructionPopulationLossPercent / 100U;
    CHECK(stores.size() == 1);
    CHECK(population.row(0).population == expectedPopulation);
}
}  // namespace

int main() {
    CHECK_NO_THROW(configuredAionBudgetAndVerticalAi());
    CHECK_NO_THROW(aionGroundInvasionReducesPopulation());
    return konbini::test::summarize("campaign systems");
}
