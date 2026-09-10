#pragma once
#include <cstdint>
namespace konbini::sim {
struct SkillContent {
    std::uint32_t maxRank = 0, maxEquipped = 0, firstExperience = 0, experienceStep = 0;
    std::uint32_t customersPerExperience = 0, baseExperience = 0, maxExperiencePerPeriod = 0;
    std::uint32_t staffFaith = 0, deliveryRadiusPermille = 0, coffeeCreditsPerStore = 0;
    std::uint32_t coffeeStoreCap = 0, bakeryRecovery = 0, snacksInfluence = 0;
    std::uint32_t qualityRevenuePermille = 0, buildDiscountPermille = 0, cameraDelayTicks = 0;
    std::uint32_t nightCooldownPermille = 0, raisedRevenuePermille = 0, raisedFaithPenalty = 0;
    std::uint32_t pulsePeriodTicks = 0, snacksDurationTicks = 0, wavePeriodTicks = 0;
    std::uint32_t waveDurationTicks = 0, waveDurationPerRank = 0;
};
void validateSkillContent(const SkillContent& content);
} // namespace konbini::sim
