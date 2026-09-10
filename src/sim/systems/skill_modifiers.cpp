#include "konbini/sim/skill_system.h"
#include <algorithm>

namespace konbini::sim {
std::int64_t skillBuildCost(const std::int64_t cost, const GameState& state,
                           const SkillContent& rules, const ChainId chain) {
    if (!state.campaign.enabled || !state.playerChain || chain != *state.playerChain) return cost;
    const auto discount = skillRank(state.campaign.skills, SkillId::ReusedShop) * rules.buildDiscountPermille;
    // Quotient/remainder avoids multiplying a potentially large currency value.
    const auto reduction = (cost / 1000) * discount + (cost % 1000) * discount / 1000;
    return std::max<std::int64_t>(1, cost - reduction);
}
std::uint32_t skillCaptureDelay(const std::uint32_t base, const GameState& state,
                               const SkillContent& rules, const ChainId targetChain) {
    return base + (state.playerChain && targetChain == *state.playerChain
        ? skillRank(state.campaign.skills, SkillId::SecurityCamera) * rules.cameraDelayTicks : 0);
}
void applySkillStoreModifiers(CampaignWorld w) {
    if (!w.state.playerChain) return;
    const auto bonus = skillRank(w.state.campaign.skills, SkillId::Delivery) *
        w.content.campaign->skills.deliveryRadiusPermille;
    const auto radius = w.economy.rules(*w.state.playerChain).zocRadiusMeters * (1000 + bonus) / 1000.0;
    for (std::size_t i = 0; i < w.stores.size(); ++i) {
        const auto s = w.stores.row(i);
        if (s.isActive && s.chain == *w.state.playerChain) w.stores.setZocRadius(i, radius);
    }
}
void applySkillRevenue(CampaignWorld w) {
    if (!w.state.playerChain) return;
    const auto& s = w.state.campaign.skills;
    const auto& r = w.content.campaign->skills;
    const auto multiplier = 1000 + skillRank(s, SkillId::QualityIngredients) * r.qualityRevenuePermille +
        skillRank(s, SkillId::RaisedBottom) * r.raisedRevenuePermille;
    for (std::size_t i = 0; i < w.stores.size(); ++i) {
        const auto store = w.stores.row(i);
        if (!store.isActive || store.chain != *w.state.playerChain) continue;
        w.stores.setRevenuePermille(i, static_cast<std::uint32_t>(std::min<std::uint64_t>(
            10000, static_cast<std::uint64_t>(store.revenuePermille) * multiplier / 1000)));
    }
}
SkillInfluence skillInfluence(const GameState& state, const SkillContent& rules) {
    if (!state.playerChain) return {};
    const auto& skills = state.campaign.skills;
    return {static_cast<int>(chainIndex(*state.playerChain)),
        skills.snacksEndTick > state.campaign.elapsedTicks
            ? skillRank(skills, SkillId::HotSnacks) * rules.snacksInfluence : 0,
        skills.waveEndTick > state.campaign.elapsedTicks};
}
} // namespace konbini::sim
