#include "konbini/sim/skill_system.h"
#include "konbini/sim/counter_rng.h"
#include <algorithm>
#include <vector>

namespace konbini::sim {
std::uint32_t nextSkillExperience(const SkillState& state, const SkillContent& rules) {
    return rules.firstExperience + (state.level - 1) * rules.experienceStep;
}
namespace {
std::vector<SkillId> availableSkills(const SkillState& state, const SkillContent& rules) {
    const auto equipped = std::ranges::count_if(state.ranks, [](auto rank) { return rank != 0; });
    std::vector<SkillId> available;
    for (std::size_t i = 0; i < kSkillCount; ++i)
        if (state.ranks[i] < rules.maxRank && (state.ranks[i] || equipped < rules.maxEquipped))
            available.push_back(static_cast<SkillId>(i));
    return available;
}
}
void awardSkillExperience(CampaignWorld w) {
    if (!w.state.playerChain || !isPlayingPhase(w.state.phase)) return;
    auto& state = w.state.campaign.skills;
    const auto& rules = w.content.campaign->skills;
    const auto player = w.economy.row(*w.state.playerChain);
    if (state.pending || !player.storeCount) return;
    auto available = availableSkills(state, rules);
    if (available.empty()) return;
    state.experience += static_cast<std::uint32_t>(std::min<std::uint64_t>(
        rules.maxExperiencePerPeriod,
        rules.baseExperience + player.customerShare / rules.customersPerExperience));
    const auto required = nextSkillExperience(state, rules);
    if (state.experience < required) return;
    state.experience -= required;
    state.pending = true;
    state.offers.fill(SkillId::Count);
    auto seed = splitMix64(w.state.worldSeed ^ 0x534B494C4C534554ull ^
                          state.level ^ (chainIndex(*w.state.playerChain) << 16));
    for (const auto rank : state.ranks) seed = splitMix64(seed ^ rank);
    // Stable seeded sampling without replacement; retry cannot reroll an offer.
    for (std::size_t i = 0; i < state.offers.size() && !available.empty(); ++i) {
        seed = splitMix64(seed);
        const auto index = static_cast<std::size_t>(seed % available.size());
        state.offers[i] = available[index];
        available.erase(available.begin() + static_cast<std::ptrdiff_t>(index));
    }
}
bool chooseSkill(CampaignWorld w, const std::uint32_t offer) {
    auto& state = w.state.campaign.skills;
    if (!state.pending || offer >= state.offers.size() || state.offers[offer] == SkillId::Count)
        return false;
    const auto id = state.offers[offer];
    auto& rank = state.ranks[static_cast<std::size_t>(id)];
    if (rank >= w.content.campaign->skills.maxRank) return false;
    ++rank;
    ++state.level;
    state.pending = false;
    state.offers.fill(SkillId::Count);
    const auto now = w.state.campaign.elapsedTicks;
    if (rank == 1) {
        // New periodic skills fire on the next gameplay tick.
        if (id == SkillId::Coffee) state.nextCoffeeTick = now;
        if (id == SkillId::HotSnacks) state.nextSnacksTick = now;
        if (id == SkillId::MindWave) state.nextWaveTick = now;
    }
    return true;
}
} // namespace konbini::sim
