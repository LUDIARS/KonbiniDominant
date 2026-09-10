#include "skill_content_parser.h"
#include <charconv>
#include <stdexcept>
namespace konbini::sim {
SkillContent parseSkillContent(const json::Value& value) {
    const auto& object = value.object();
    if (object.size() != 24) throw std::invalid_argument("unknown or missing skill field");
    const auto integer = [&](const char* name) {
        const auto found = object.find(name);
        if (found == object.end() || !found->second.isNumber())
            throw std::invalid_argument("missing integer skill field");
        const auto text = found->second.numberLexeme();
        std::uint32_t result = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
            throw std::invalid_argument("invalid skill integer");
        return result;
    };
    SkillContent s{
        integer("maxRank"), integer("maxEquipped"), integer("firstExperience"), integer("experienceStep"),
        integer("customersPerExperience"), integer("baseExperience"), integer("maxExperiencePerPeriod"),
        integer("staffFaith"), integer("deliveryRadiusPermille"), integer("coffeeCreditsPerStore"),
        integer("coffeeStoreCap"), integer("bakeryRecovery"), integer("snacksInfluence"),
        integer("qualityRevenuePermille"), integer("buildDiscountPermille"), integer("cameraDelayTicks"),
        integer("nightCooldownPermille"), integer("raisedRevenuePermille"), integer("raisedFaithPenalty"),
        integer("pulsePeriodTicks"), integer("snacksDurationTicks"), integer("wavePeriodTicks"),
        integer("waveDurationTicks"), integer("waveDurationPerRank")
    };
    validateSkillContent(s);
    return s;
}
}
