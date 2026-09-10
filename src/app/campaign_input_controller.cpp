#include "konbini/app/campaign_input_controller.h"
#include <algorithm>
namespace konbini::app {
std::uint32_t CampaignInputController::firstEmpty(const sim::RenderSnapshot& snapshot,const sim::FacilityId facility) const {
    for(std::uint32_t slot=0;slot<snapshot.hud().campaign.slots;++slot)
        if(std::ranges::none_of(snapshot.stores(),[&](const auto& s){return s.facilityId==facility && s.verticalSlot==slot;})) return slot;
    return snapshot.hud().campaign.slots;
}
void CampaignInputController::updateFloor(const FrameInput& input,const sim::RenderSnapshot& snapshot,const std::optional<sim::FacilityId> selected) {
    if(!snapshot.hud().campaign.enabled || snapshot.hud().campaign.reachedPhase<2) {floor_=0;return;}
    if(visibleDimension_!=snapshot.hud().campaign.visibleDimension) {
        visibleDimension_=snapshot.hud().campaign.visibleDimension;
        floor_=0;
    }
    if(input.floorDown && floor_>0) --floor_;
    if(input.floorUp && floor_+1<snapshot.hud().campaign.slots) ++floor_;
    if(input.groundFloor) floor_=0;
    if(input.nextFreeFloor && selected) floor_=std::min(snapshot.hud().campaign.slots-1,firstEmpty(snapshot,*selected));
}
std::vector<sim::PlayerCommand> CampaignInputController::actions(const FrameInput& input,
    const sim::RenderSnapshot& snapshot,const std::optional<sim::FacilityId> selected,
    const std::uint64_t tick,CommandComposer& commands) {
    std::vector<sim::PlayerCommand> output;
    const auto& hud=snapshot.hud();
    if(hud.campaign.skills.pending || !hud.playerChain || !sim::isPlayingPhase(hud.phase)) return output;
    if(input.buildRequested && selected) output.push_back(commands.placeStore(*hud.playerChain,*selected,tick,floor_));
    if(!hud.campaign.enabled) return output;
    const auto append=[&](sim::CampaignAction action,std::uint32_t dimension=0){
        output.push_back(commands.campaignAction(*hud.playerChain,action,selected.value_or(sim::FacilityId{}),floor_,dimension,tick));
    };
    if(input.imageStrategy) append(sim::CampaignAction::ImageStrategy);
    if(input.invertStore) append(sim::CampaignAction::InvertStore);
    if(input.nextDimension) {
        std::vector<std::uint32_t> active;
        for(const auto& d:hud.campaign.dimensions) if(d.active) active.push_back(d.id);
        if(!active.empty()) {
            auto found=std::ranges::find(active,hud.campaign.visibleDimension);
            const auto next=found==active.end() ? 0 : (static_cast<std::size_t>(found-active.begin())+1)%active.size();
            append(sim::CampaignAction::ViewDimension,active[next]);
        }
    }
    if(input.escapeDimension) append(sim::CampaignAction::Escape);
    if(input.buildHeld && !input.nextDimension && !input.escapeDimension && selected &&
       hud.campaign.reachedPhase>=2 && tick>=nextBuildTick_) {
        const auto slot=firstEmpty(snapshot,*selected);
        if(slot<hud.campaign.slots) {
            floor_=slot;
            output.push_back(commands.placeStore(*hud.playerChain,*selected,tick,slot));
        }
        nextBuildTick_=tick+1;
    }
    return output;
}
}
