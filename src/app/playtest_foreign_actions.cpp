#include "konbini/app/playtest_pilot.h"
#include <algorithm>
namespace konbini::app {
using namespace sim;
PlaytestPilot::Decision PlaytestPilot::conquerForeign(const Context& ctx) {
    const auto& snapshot=ctx.snapshot;auto& commands=ctx.commands;
    const auto& hud=snapshot.hud();const auto& campaign=hud.campaign;
    const auto chain=*hud.playerChain;const auto tick=hud.completedTicks;
    const auto action=[&](sim::CampaignAction kind,sim::FacilityId facility={},std::uint32_t floor=0,std::uint32_t dimension=0) {
        return commands.campaignAction(chain,kind,facility,floor,dimension,tick);
    };
    const auto anchor=[&](sim::FacilityId id) {return ctx.anchor(id);};
    const auto occupied=[&](sim::FacilityId id,std::uint32_t floor) {return ctx.occupied(id,floor);};
    if(hud.phase==GamePhase::Phase3) {
        const auto target=std::ranges::find_if(campaign.dimensions,[](const auto& d){return d.active && d.id>0;});
        if(target!=campaign.dimensions.end()) {
            foreign_=target->id;
            if(campaign.visibleDimension==0 && neededOrigin_) {
                const auto [key,floor]=*neededOrigin_;
                if(originStores_.contains(*neededOrigin_)) neededOrigin_.reset();
                else for(const auto& f:snapshot.facilities()) if(f.figmentumKey.value()==key && !occupied(f.id,floor)) {
                    if(hud.cashCredits>=hud.chains[chainIndex(chain)].buildCost)
                        return commands.placeStore(chain,f.id,tick,floor);
                    return bt::Status::Running;
                }
                // Remember blocked origin slots rather than toggling worlds forever.
                if(neededOrigin_) blockedOrigin_.insert(*neededOrigin_);
                neededOrigin_.reset();
            }
            if(campaign.visibleDimension!=foreign_) return action(CampaignAction::ViewDimension,{},0,foreign_);
            for(const auto& store:snapshot.stores()) {
                if(store.chain==chain || store.chain==ChainId::Aion || store.isAntiStore) continue;
                const auto key=std::pair{anchor(store.facilityId),store.verticalSlot};
                if(!originStores_.contains(key)) continue;
                if(store.faith==campaign.faithMax && !campaign.inversionCooldown && hud.cashCredits>=campaign.inversionCost)
                    return action(CampaignAction::InvertStore,store.facilityId,store.verticalSlot);
                return bt::Status::Running; // Ordinary faith growth; no state override.
            }
            for(const auto& store:snapshot.stores()) if(store.chain!=chain && store.chain!=ChainId::Aion && !store.isAntiStore) {
                const auto key=std::pair{anchor(store.facilityId),store.verticalSlot};
                if(blockedOrigin_.contains(key)) continue;
                neededOrigin_=key;
                return action(CampaignAction::ViewDimension,{},0,0);
            }
        }
    }
    return {};
}
}
