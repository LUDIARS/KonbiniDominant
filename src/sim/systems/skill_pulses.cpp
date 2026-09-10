#include "konbini/sim/skill_system.h"
#include <algorithm>
namespace konbini::sim {
void advanceSkillPulses(CampaignWorld w) {
    if (!w.state.playerChain) return;
    auto& state = w.state.campaign.skills;
    const auto& rules = w.content.campaign->skills;
    const auto now = w.state.campaign.elapsedTicks;
    const auto period = [&](std::uint32_t base) {
        return base * (1000 - skillRank(state, SkillId::NightShift) * rules.nightCooldownPermille) / 1000;
    };
    const auto coffee = skillRank(state, SkillId::Coffee);
    if (coffee && now >= state.nextCoffeeTick) {
        state.coffeePayout = static_cast<std::int64_t>(coffee) * rules.coffeeCreditsPerStore *
            std::min(w.economy.row(*w.state.playerChain).storeCount, rules.coffeeStoreCap);
        w.economy.creditRevenue(*w.state.playerChain, state.coffeePayout);
        state.coffeeAtTick = now;
        state.nextCoffeeTick = now + period(rules.pulsePeriodTicks);
    }
    if (skillRank(state, SkillId::HotSnacks) && now >= state.nextSnacksTick) {
        state.snacksEndTick = now + rules.snacksDurationTicks;
        state.nextSnacksTick = now + period(rules.pulsePeriodTicks);
    }
    const auto wave = skillRank(state, SkillId::MindWave);
    if (wave && now >= state.nextWaveTick) {
        state.waveEndTick = now + rules.waveDurationTicks + wave * rules.waveDurationPerRank;
        state.nextWaveTick = now + period(rules.wavePeriodTicks);
    }
    if (now % w.content.simulation.economyPeriodTicks != 0) return;
    const auto recovery = skillRank(state, SkillId::Bakery) * rules.bakeryRecovery;
    for (std::size_t i = 0; i < w.population.size(); ++i) {
        const auto cell = w.population.row(i);
        if (!cell.assignedStore) continue;
        const auto store = w.stores.find(*cell.assignedStore);
        if (!store || !w.stores.row(*store).isActive ||
            w.stores.row(*store).chain != *w.state.playerChain) continue;
        w.population.setPopulation(i, cell.population + std::min(recovery, cell.capacity - cell.population));
    }
}
} // namespace konbini::sim
