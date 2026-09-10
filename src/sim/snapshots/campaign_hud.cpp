#include "konbini/sim/campaign_hud.h"
#include "konbini/sim/skill_system.h"
#include <algorithm>
namespace konbini::sim {
CampaignHud makeCampaignHud(const GameState& state,const CampaignContent& c,const StoreTable& stores) {
    CampaignHud hud; const auto& s=state.campaign;
    hud.skills=s.skills; hud.skillRules=c.skills; hud.nextExperience=nextSkillExperience(s.skills,c.skills);
    hud.enabled=true; hud.reachedPhase=s.reachedPhase; hud.visibleDimension=s.visibleDimension;
    hud.destroyedForeign=s.destroyedForeign; hud.highestStack=s.highestStack; hud.elapsedTicks=s.elapsedTicks;
    const auto remain=[](std::uint64_t end,std::uint64_t now){return end>now?end-now:0;};
    std::uint32_t duration=0;
    if(state.phase==GamePhase::Phase2) duration=c.vertical.durationTicks;
    if(state.phase==GamePhase::BossWarning) duration=c.aion.warningTicks;
    if(state.phase==GamePhase::Boss) duration=c.aion.manifestationTicks;
    hud.phaseRemaining=remain(duration,state.phaseTicks);
    hud.imageCooldown=remain(s.imageReadyTick,s.elapsedTicks);
    hud.inversionCooldown=remain(s.inversionReadyTick,s.elapsedTicks);
    hud.escapeCooldown=remain(s.escapeReadyTick,s.elapsedTicks);
    hud.slots=c.vertical.slots; hud.costStepPermille=c.vertical.costStepPermille;
    hud.floorHeightMeters=c.vertical.floorHeightMeters; hud.faithMax=c.vertical.faithMax;
    hud.imageCost=c.vertical.imageCost; hud.inversionCost=c.dimensions.inversionCost;
    hud.escapeCost=c.dimensions.escapeCost;
    hud.timePasteEvents=s.timePasteEvents; hud.maxValueEvents=s.maxValueEvents;
    for(const auto& d:s.dimensions) {
        DimensionHud view; view.seed=d.seed; view.id=d.id; view.active=d.status==DimensionStatus::Active;
        view.maxValueRemaining=remain(d.maxValueAtTick,s.elapsedTicks);
        view.energyRemaining=remain(d.energyEndTick,s.elapsedTicks);
        for(std::size_t i=0;i<stores.size();++i) {
            const auto store=stores.row(i);
            if(!store.isActive || store.dimension!=d.id) continue;
            if(store.chain==ChainId::Aion) ++view.bossStores;
            else if(state.playerChain && store.chain==*state.playerChain) ++view.playerStores;
            else ++view.rivalStores;
        }
        hud.dimensions.push_back(view);
    }
    return hud;
}
}
