#include "konbini/sim/skill_content.h"
#include <stdexcept>
namespace konbini::sim {
void validateSkillContent(const SkillContent& s) {
    if (s.maxRank != 5 || s.maxEquipped != 6 || !s.firstExperience ||
        s.firstExperience > 10000 || !s.experienceStep || s.experienceStep > 1000 ||
        !s.customersPerExperience || !s.baseExperience || s.maxExperiencePerPeriod < s.baseExperience ||
        s.maxExperiencePerPeriod > 1000 || !s.staffFaith || s.staffFaith > 3 ||
        !s.deliveryRadiusPermille || s.deliveryRadiusPermille > 200 ||
        !s.coffeeCreditsPerStore || s.coffeeCreditsPerStore > 1000 ||
        !s.coffeeStoreCap || s.coffeeStoreCap > 100 || !s.bakeryRecovery || s.bakeryRecovery > 20 ||
        !s.snacksInfluence || s.snacksInfluence > 10 ||
        !s.qualityRevenuePermille || s.qualityRevenuePermille > 300 ||
        !s.buildDiscountPermille || s.buildDiscountPermille > 100 ||
        !s.cameraDelayTicks || s.cameraDelayTicks > 100 ||
        !s.nightCooldownPermille || s.nightCooldownPermille > 100 ||
        !s.raisedRevenuePermille || s.raisedRevenuePermille > 500 ||
        !s.raisedFaithPenalty || s.raisedFaithPenalty > 3 ||
        s.pulsePeriodTicks < 40 || s.pulsePeriodTicks > 600 ||
        !s.snacksDurationTicks || s.snacksDurationTicks > s.pulsePeriodTicks / 2 ||
        s.wavePeriodTicks < 100 || s.wavePeriodTicks > 600 ||
        !s.waveDurationTicks || s.waveDurationTicks > 30 ||
        !s.waveDurationPerRank || s.waveDurationPerRank > 5 ||
        s.waveDurationTicks + s.maxRank * s.waveDurationPerRank > s.wavePeriodTicks / 2) {
        throw std::invalid_argument("invalid skill rules");
    }
}
} // namespace konbini::sim
