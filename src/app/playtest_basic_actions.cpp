#include "konbini/app/playtest_pilot.h"
#include <algorithm>
namespace konbini::app {
using namespace sim;
PlaytestPilot::Decision PlaytestPilot::selectChain(const Context& ctx) {
    if(ctx.snapshot.hud().phase!=GamePhase::ChainSelect) return {};
    return ctx.commands.selectChain(ChainId::Losan,ctx.snapshot.completedTicks());
}
PlaytestPilot::Decision PlaytestPilot::chooseSkill(const Context& ctx) {
    const auto& snapshot=ctx.snapshot;auto& commands=ctx.commands;
    const auto& hud=snapshot.hud();const auto& campaign=hud.campaign;
    const auto chain=*hud.playerChain;const auto tick=hud.completedTicks;
    const auto action=[&](sim::CampaignAction kind,sim::FacilityId facility={},std::uint32_t floor=0,std::uint32_t dimension=0) {
        return commands.campaignAction(chain,kind,facility,floor,dimension,tick);
    };
    if(campaign.skills.pending) {
        constexpr int preference[]={8,4,3,2,5,7,10,6,1,9,11};
        std::uint32_t best=0;int score=-1;
        for(std::uint32_t i=0;i<campaign.skills.offers.size();++i) {
            const auto id=static_cast<std::size_t>(campaign.skills.offers[i]);
            if(id<kSkillCount && preference[id]>score) {best=i;score=preference[id];}
        }
        return action(CampaignAction::ChooseSkill,{},0,best);
    }
    return {};
}
PlaytestPilot::Decision PlaytestPilot::escapeBoss(const Context& ctx) {
    const auto& snapshot=ctx.snapshot;auto& commands=ctx.commands;
    const auto& hud=snapshot.hud();const auto& campaign=hud.campaign;
    const auto chain=*hud.playerChain;const auto tick=hud.completedTicks;
    const auto action=[&](sim::CampaignAction kind,sim::FacilityId facility={},std::uint32_t floor=0,std::uint32_t dimension=0) {
        return commands.campaignAction(chain,kind,facility,floor,dimension,tick);
    };
    if(hud.phase==GamePhase::Boss || hud.phase==GamePhase::BossWarning) {
        const auto active=std::ranges::count_if(campaign.dimensions,[](const auto& d){return d.active;});
        if(!campaign.escapeCooldown && active<6 &&
           hud.cashCredits>=campaign.escapeCost+hud.chains[chainIndex(chain)].buildCost)
            return action(CampaignAction::Escape);
    }
    return {};
}
}
