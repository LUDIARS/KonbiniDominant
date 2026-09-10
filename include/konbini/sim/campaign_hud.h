#pragma once
#include "konbini/sim/campaign_content.h"
#include "konbini/sim/game_state.h"
#include "konbini/sim/store_table.h"
namespace konbini::sim {
struct DimensionHud {
    std::uint32_t id=0, playerStores=0, rivalStores=0, bossStores=0;
    bool active=false;
    std::uint64_t seed=0;
    std::uint64_t maxValueRemaining=0, energyRemaining=0;
};
struct CampaignHud {
    bool enabled=false;
    SkillState skills;
    SkillContent skillRules;
    std::uint32_t nextExperience=0;
    std::uint32_t reachedPhase=0, visibleDimension=0, destroyedForeign=0, highestStack=0;
    std::uint64_t elapsedTicks=0, phaseRemaining=0, imageCooldown=0, inversionCooldown=0, escapeCooldown=0;
    std::uint32_t slots=0, costStepPermille=0, imageCost=0, inversionCost=0, escapeCost=0, faithMax=0;
    std::uint32_t timePasteEvents=0, maxValueEvents=0;
    double floorHeightMeters=0;
    std::vector<DimensionHud> dimensions;
};
CampaignHud makeCampaignHud(const GameState& state,const CampaignContent& content,const StoreTable& stores);
}
