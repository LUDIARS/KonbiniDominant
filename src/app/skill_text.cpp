#include "konbini/app/skill_text.h"
#include <array>
#include <stdexcept>
namespace konbini::app {
std::string skillLabel(const sim::SkillId id) {
    constexpr std::array names{"MULTILINGUAL STAFF", "DELIVERY", "COFFEE MACHINE", "BREAD AND DONUTS",
        "HOT SNACKS", "QUALITY INGREDIENTS", "REUSED SHOP", "SECURITY CAMERA", "NIGHT SHIFT",
        "RAISED BOTTOM", "MIND WAVE"};
    const auto index = static_cast<std::size_t>(id);
    if (index >= names.size()) throw std::invalid_argument("unknown skill label");
    return names[index];
}
std::string skillDescription(const sim::SkillId id, const std::uint32_t rank, const sim::SkillContent& r) {
    const auto n=[](auto value){return std::to_string(value);};
    switch(id) {
    case sim::SkillId::MultilingualStaff: return "FAITH GROWTH +" + n(rank*r.staffFaith);
    case sim::SkillId::Delivery: return "REACH +" + n(rank*r.deliveryRadiusPermille/10) + " PERCENT";
    case sim::SkillId::Coffee: return "PERIODIC CASH +" + n(rank*r.coffeeCreditsPerStore) + " PER STORE";
    case sim::SkillId::Bakery: return "SERVED POPULATION RECOVERY +" + n(rank*r.bakeryRecovery);
    case sim::SkillId::HotSnacks: return "PERIODIC ATTRACTION +" + n(rank*r.snacksInfluence);
    case sim::SkillId::QualityIngredients: return "REVENUE +" + n(rank*r.qualityRevenuePermille/10) + " PERCENT";
    case sim::SkillId::ReusedShop: return "BUILD COST -" + n(rank*r.buildDiscountPermille/10) + " PERCENT";
    case sim::SkillId::SecurityCamera: return "ENCIRCLEMENT SURVIVAL +" + n(rank*r.cameraDelayTicks/10) + "S";
    case sim::SkillId::NightShift: return "PERIODIC COOLDOWNS -" + n(rank*r.nightCooldownPermille/10) + " PERCENT";
    case sim::SkillId::RaisedBottom: return "REVENUE +" + n(rank*r.raisedRevenuePermille/10) +
        " PERCENT / FAITH GROWTH -" + n(rank*r.raisedFaithPenalty);
    case sim::SkillId::MindWave: return "STEAL NEARBY CUSTOMERS FOR " +
        n((r.waveDurationTicks+rank*r.waveDurationPerRank)/10) + "." +
        n((r.waveDurationTicks+rank*r.waveDurationPerRank)%10) + "S";
    case sim::SkillId::Count: break;
    }
    throw std::invalid_argument("unknown skill description");
}
}
