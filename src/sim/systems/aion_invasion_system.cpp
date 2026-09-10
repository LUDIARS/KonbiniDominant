#include "konbini/sim/aion_system.h"
#include "konbini/sim/dimension_system.h"
#include "konbini/sim/population_change_system.h"
#include "konbini/sim/vertical_placement.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace konbini::sim {
namespace {
std::vector<FacilityRow> lots(CampaignWorld w,const std::uint32_t dimension) {
    std::vector<FacilityRow> result;
    for(std::size_t i=0;i<w.facilities.size();++i) {
        const auto f=w.facilities.row(i);
        if(f.dimension==dimension && f.isBuildable) result.push_back(f);
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.figmentumKey<b.figmentumKey;});
    return result;
}
void invade(CampaignWorld w,const std::uint32_t dimension) {
    const auto candidates=lots(w,dimension);
    std::optional<PlaceStoreCommand> best;
    double bestScore=std::numeric_limits<double>::max();
    Vec3 anchor{};
    for(std::size_t i=0;i<w.stores.size();++i) {
        const auto s=w.stores.row(i);
        if(s.isActive && s.chain==ChainId::Aion && s.dimension==dimension) {anchor=s.positionMeters;break;}
    }
    for(const auto& f:candidates) {
        const auto slot=w.stores.firstEmptySlot(f.id,w.content.campaign->vertical.slots);
        PlaceStoreCommand command{{w.state.completedTicks,CommandSourcePriority::Ai,f.id.value().index},
            ChainId::Aion,f.id,slot};
        if(validatePlacement(command,w.state,w.facilities,w.stores,w.economy).failure!=PlacementFailure::None) continue;
        bool alreadyBoss=false;
        for(std::size_t i=0;i<w.stores.size();++i) {
            const auto s=w.stores.row(i);
            if(s.isActive && s.facilityId==f.id && s.chain==ChainId::Aion) {alreadyBoss=true;break;}
        }
        const double dx=f.positionMeters.x-anchor.x,dz=f.positionMeters.z-anchor.z;
        const double score=dx*dx+dz*dz+slot*50.0+(alreadyBoss?1e9:0);
        if(score<bestScore) {bestScore=score;best=command;}
    }
    if(!best) return;
    const auto result=commitPlacementAtomically(
        *best,w.state,w.facilities,w.stores,w.economy,w.identities.stores());
    if(result.failure!=PlacementFailure::None)
        throw std::logic_error("validated Aion invasion placement failed");
    if(best->verticalSlot==0 && result.replacedIntactFacility) {
        applyPopulationLoss(w.population,best->facilityId,
            w.content.phase1->destructionPopulationLossPercent);
    }
}
void pasteHistory(CampaignWorld w) {
    const auto& rules=w.content.campaign->aion;
    auto& state=w.state.campaign;
    if(state.elapsedTicks<rules.historyTicks) return;
    const auto frame=std::ranges::find_if(state.history,[&](const auto& h){
        return h.tick==state.elapsedTicks-rules.historyTicks;});
    if(frame==state.history.end()) return;
    const auto past=frame->stores;
    bool pasted=false;
    std::uint32_t count=0;
    for(const auto& dimension:state.dimensions) {
        if(count>=rules.pasteLimit) break;
        if(dimension.status!=DimensionStatus::Active) continue;
        const auto candidates=lots(w,dimension.id);
        if(candidates.empty()) continue;
        for(const auto& s:past) {
            if(count>=rules.pasteLimit) break;
            if(s.dimension!=dimension.id) continue;
            const auto source=std::ranges::find_if(candidates,[&](const auto& f){return f.figmentumKey.value()==s.anchor;});
            if(source==candidates.end()) continue;
            const auto offset=(static_cast<std::size_t>(source-candidates.begin())+rules.pasteLotOffset)%candidates.size();
            const auto target=candidates[offset].id;
            if(s.verticalSlot && !w.stores.findAt(target,s.verticalSlot-1)) continue;
            if(!w.economy.canAfford(ChainId::Aion,verticalBuildCost(w.economy.rules(ChainId::Aion).buildCostCredits,
                s.verticalSlot,w.content.campaign->vertical))) continue;
            if(const auto occupant=w.stores.findAt(target,s.verticalSlot))
                destroyCampaignStore(w,w.stores.row(*occupant).id);
            const PlaceStoreCommand command{{w.state.completedTicks,CommandSourcePriority::Ai,count},
                ChainId::Aion,target,s.verticalSlot};
            const auto result=commitPlacementAtomically(command,w.state,w.facilities,w.stores,w.economy,w.identities.stores());
            if(result.failure!=PlacementFailure::None) throw std::logic_error("validated time paste failed");
            ++count; pasted=true;
        }
    }
    if(pasted) ++state.timePasteEvents;
}
}
// @implements spec/feature/full-campaign-baseline.md Aion
void advanceAionInvasion(CampaignWorld w) {
    if(w.state.phase!=GamePhase::Boss) return;
    auto& state=w.state.campaign; const auto& rules=w.content.campaign->aion;
    if(state.elapsedTicks>=state.nextBossSpawnTick) {
        for(const auto& dimension:state.dimensions)
            if(dimension.status==DimensionStatus::Active) invade(w,dimension.id);
        state.nextBossSpawnTick=state.elapsedTicks+rules.spawnPeriodTicks;
    }
    if(state.elapsedTicks>=state.nextTimePasteTick) {
        pasteHistory(w); state.nextTimePasteTick=state.elapsedTicks+rules.timePeriodTicks;
    }
    AionHistoryFrame frame; frame.tick=state.elapsedTicks;
    for(std::size_t i=0;i<w.stores.size();++i) {
        const auto s=w.stores.row(i);
        if(!s.isActive || s.chain!=ChainId::Aion) continue;
        const auto f=w.facilities.row(*w.facilities.find(s.facilityId));
        frame.stores.push_back({s.dimension,s.verticalSlot,f.figmentumKey.value()});
    }
    std::sort(frame.stores.begin(),frame.stores.end(),[](const auto& a,const auto& b){
        if(a.dimension!=b.dimension) return a.dimension<b.dimension;
        if(a.anchor!=b.anchor) return a.anchor<b.anchor;
        return a.verticalSlot<b.verticalSlot;
    });
    state.history.push_back(std::move(frame));
    std::erase_if(state.history,[&](const auto& h){return h.tick+rules.historyTicks<state.elapsedTicks;});
}
}
