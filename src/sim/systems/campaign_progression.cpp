#include "konbini/sim/campaign_progression.h"
#include "konbini/sim/dimension_system.h"
#include "konbini/sim/phase1_outcome_system.h"
#include "konbini/sim/skill_system.h"
#include <algorithm>
#include <stdexcept>
#include <tuple>
namespace konbini::sim {
namespace {
void finish(GameState& state,const MatchOutcome outcome,const MatchEndReason reason) {
    state.phase=GamePhase::Result; state.outcome=outcome; state.endReason=reason;
}
void transition(GameState& state,const GamePhase phase,const std::uint32_t reached) {
    state.phase=phase; state.phaseTicks=0; state.campaign.reachedPhase=reached;
}
std::uint32_t tallestStack(const StoreTable& stores) {
    std::vector<StoreRow> rows;
    for(std::size_t i=0;i<stores.size();++i) if(stores.row(i).isActive) rows.push_back(stores.row(i));
    std::sort(rows.begin(),rows.end(),[](const auto& a,const auto& b){
        return std::tuple{a.facilityId,a.verticalSlot}<std::tuple{b.facilityId,b.verticalSlot};});
    std::uint32_t best=0,consecutive=0; FacilityId facility{};
    for(const auto& store:rows) {
        if(store.facilityId!=facility) {facility=store.facilityId;consecutive=0;}
        if(store.verticalSlot==consecutive) ++consecutive;
        best=std::max(best,consecutive);
    }
    return best;
}
}
// @implements spec/feature/full-campaign-baseline.md Progression
// @implements spec/feature/full-campaign-baseline.md Aion
void resolveCampaignProgression(CampaignWorld w) {
    if(!isPlayingPhase(w.state.phase) || !w.state.playerChain) return;
    const auto player=w.economy.row(*w.state.playerChain);
    if(w.state.campaign.reachedPhase>=2)
        w.state.campaign.highestStack=std::max(w.state.campaign.highestStack,tallestStack(w.stores));
    if(w.state.phase==GamePhase::Boss) {
        // Collapsed dimensions have already removed their stores and population.
        if(player.storeCount==0) {finish(w.state,MatchOutcome::Lose,MatchEndReason::AllStoresLost);return;}
        if(w.state.phaseTicks>=w.content.campaign->aion.manifestationTicks)
            finish(w.state,MatchOutcome::Win,MatchEndReason::AionSurvived);
        return;
    }
    if(player.storeCount==0 && !w.economy.canAfford(*w.state.playerChain,skillBuildCost(w.economy.rules(*w.state.playerChain).buildCostCredits,
        w.state,w.content.campaign->skills,*w.state.playerChain))) {
        finish(w.state,MatchOutcome::Lose,MatchEndReason::NoCapital); return;
    }
    switch(w.state.phase) {
    case GamePhase::Phase1: {
        auto result=w.state;
        resolvePhase1Outcome(result,w.economy,w.population,*w.content.phase1);
        if(result.phase==GamePhase::Result && result.endReason!=MatchEndReason::NoCapital) transition(w.state,GamePhase::Phase2,2);
        break;
    }
    case GamePhase::Phase2:
        if(w.state.campaign.highestStack>=w.content.campaign->vertical.slots ||
           w.state.phaseTicks>=w.content.campaign->vertical.durationTicks) {
            transition(w.state,GamePhase::Phase3,3);
            for(std::size_t i=0;i<kFirstPlayableChainCount;++i) {
                const auto chain=static_cast<ChainId>(i);
                if(chain==*w.state.playerChain) continue;
                const auto dimension=createCampaignDimension(w,true);
                seedForeignOpponents(w,dimension,chain);
            }
        }
        break;
    case GamePhase::Phase3:
        if(w.state.campaign.destroyedForeign>=w.content.campaign->dimensions.foreignCount)
            transition(w.state,GamePhase::BossWarning,4);
        break;
    case GamePhase::BossWarning:
        if(w.state.phaseTicks>=w.content.campaign->aion.warningTicks) {
            transition(w.state,GamePhase::Boss,4);
            if (!w.economy.activateWithCash(
                    ChainId::Aion, w.content.campaign->aion.budgetCredits)) {
                throw std::logic_error("Aion economy was active before boss entry");
            }
            w.state.campaign.nextBossSpawnTick=w.state.campaign.elapsedTicks+1;
            w.state.campaign.nextTimePasteTick=w.state.campaign.elapsedTicks+w.content.campaign->aion.timePeriodTicks;
            if(player.storeCount==0) finish(w.state,MatchOutcome::Lose,MatchEndReason::AllStoresLost);
        }
        break;
    default: break;
    }
}
}
