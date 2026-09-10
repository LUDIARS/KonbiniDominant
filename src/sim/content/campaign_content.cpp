#include "konbini/sim/campaign_content.h"
#include <cmath>
#include <stdexcept>
namespace konbini::sim {
// @implements spec/feature/full-campaign-baseline.md
void validateCampaignContent(const CampaignContent& c) {
    validateSkillContent(c.skills);
    const auto& v=c.vertical; const auto& d=c.dimensions; const auto& a=c.aion;
    if (!v.durationTicks || v.durationTicks>36000 || v.slots!=256 ||
        !v.costStepPermille || v.costStepPermille>1000 ||
        !std::isfinite(v.floorHeightMeters) || v.floorHeightMeters<=0 || v.floorHeightMeters>10 ||
        v.faithMax!=100 || v.startingFaith>v.faithMax || !v.faithGrowth || v.faithGrowth>10 ||
        v.faithNeighborLimit>10 || v.triangleFaith>10 || v.heightFaithLimit>10 ||
        !v.imageCost || !v.imageCooldownTicks || !v.imageFaith || v.imageFaith>v.faithMax ||
        d.foreignCount!=2 || d.seedVersion!=1 || !d.initialRivalStores || d.initialRivalStores>16 ||
        !d.inversionCost || !d.inversionCooldownTicks || !d.energyDurationTicks ||
        d.energyRevenuePermille<1000 || d.energyRevenuePermille>3000 || d.energyFaith>10 ||
        d.maxActive<3 || d.maxActive>8 || !d.escapeCost || !d.escapeCooldownTicks ||
        !a.warningTicks || a.warningTicks>100 || a.manifestationTicks!=300 ||
        !a.spawnPeriodTicks || a.spawnPeriodTicks>30 || !a.maxValueWarningTicks ||
        a.maxValueWarningTicks>100 || !a.budgetCredits || !a.buildCostCredits ||
        a.budgetCredits/a.buildCostCredits<1000 ||
        !std::isfinite(a.zocRadiusMeters) || a.zocRadiusMeters<=0 || a.zocRadiusMeters>100 ||
        !a.timePeriodTicks || a.timePeriodTicks>100 || !a.historyTicks ||
        a.historyTicks>a.timePeriodTicks || !a.pasteLotOffset || !a.pasteLimit || a.pasteLimit>8) {
        throw std::invalid_argument("invalid campaign rules");
    }
}
}
