#pragma once
#include "konbini/sim/skill_state.h"
#include "konbini/sim/skill_content.h"
namespace konbini::sim {
template<class Writer>
void writeSkillFields(Writer& out,const SkillState& s,const SkillContent& r) {
    out.u32(s.level); out.u32(s.experience); out.u8(s.pending?1:0);
    for(const auto rank:s.ranks) out.u32(rank);
    for(const auto offer:s.offers) out.u8(static_cast<std::uint8_t>(offer));
    for(const auto tick:{s.nextCoffeeTick,s.nextSnacksTick,s.nextWaveTick,
        s.snacksEndTick,s.waveEndTick,s.coffeeAtTick}) out.u64(tick);
    out.i64(s.coffeePayout);
    for(const auto value:{r.maxRank,r.maxEquipped,r.firstExperience,r.experienceStep,
        r.customersPerExperience,r.baseExperience,r.maxExperiencePerPeriod,r.staffFaith,
        r.deliveryRadiusPermille,r.coffeeCreditsPerStore,r.coffeeStoreCap,r.bakeryRecovery,
        r.snacksInfluence,r.qualityRevenuePermille,r.buildDiscountPermille,r.cameraDelayTicks,
        r.nightCooldownPermille,r.raisedRevenuePermille,r.raisedFaithPenalty,r.pulsePeriodTicks,
        r.snacksDurationTicks,r.wavePeriodTicks,r.waveDurationTicks,r.waveDurationPerRank}) out.u32(value);
}
}
