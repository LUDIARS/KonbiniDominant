#pragma once
#include "konbini/sim/campaign_world.h"
namespace konbini::sim {
std::uint32_t nextSkillExperience(const SkillState& state, const SkillContent& rules);
void awardSkillExperience(CampaignWorld world);
bool chooseSkill(CampaignWorld world, std::uint32_t offer);
void applySkillStoreModifiers(CampaignWorld world);
void applySkillRevenue(CampaignWorld world);
void advanceSkillPulses(CampaignWorld world);
SkillInfluence skillInfluence(const GameState& state, const SkillContent& rules);
std::int64_t skillBuildCost(std::int64_t cost, const GameState& state,
                           const SkillContent& rules, ChainId chain);
std::uint32_t skillCaptureDelay(std::uint32_t base, const GameState& state,
                               const SkillContent& rules, ChainId targetChain);
} // namespace konbini::sim
