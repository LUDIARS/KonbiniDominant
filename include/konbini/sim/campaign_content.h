#pragma once
#include <cstdint>
#include "konbini/sim/skill_content.h"
namespace konbini::sim {
struct VerticalContent {
    std::uint32_t durationTicks=0, slots=0, costStepPermille=0;
    double floorHeightMeters=0;
    std::uint32_t faithMax=0, startingFaith=0, faithGrowth=0, faithNeighborLimit=0;
    std::uint32_t triangleFaith=0, heightFaithLimit=0;
    std::uint32_t imageCost=0, imageCooldownTicks=0, imageFaith=0;
};
struct DimensionContent {
    std::uint32_t foreignCount=0, seedVersion=0, initialRivalStores=0;
    std::uint32_t inversionCost=0, inversionCooldownTicks=0;
    std::uint32_t energyDurationTicks=0, energyRevenuePermille=0, energyFaith=0;
    std::uint32_t maxActive=0, escapeCost=0, escapeCooldownTicks=0;
};
struct AionContent {
    std::uint32_t warningTicks=0, manifestationTicks=0, spawnPeriodTicks=0;
    std::uint32_t maxValueWarningTicks=0, budgetCredits=0, buildCostCredits=0;
    double zocRadiusMeters=0;
    std::uint32_t timePeriodTicks=0, historyTicks=0, pasteLotOffset=0, pasteLimit=0;
};
struct CampaignContent {
    VerticalContent vertical;
    DimensionContent dimensions;
    AionContent aion;
    SkillContent skills;
};
void validateCampaignContent(const CampaignContent& content);
} // namespace konbini::sim
