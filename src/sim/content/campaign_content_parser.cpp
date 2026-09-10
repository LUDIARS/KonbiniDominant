#include "campaign_content_parser.h"
#include "skill_content_parser.h"
#include <charconv>
#include <cmath>
#include <stdexcept>
#include <string>
namespace konbini::sim {
namespace {
using Object=json::Value::Object;
const json::Value& field(const Object& o, const char* key) {
    const auto found=o.find(key);
    if(found==o.end()) throw std::invalid_argument(std::string("missing campaign field: ")+key);
    return found->second;
}
std::uint32_t integer(const Object& o, const char* key) {
    const auto& value=field(o,key);
    if(!value.isNumber()) throw std::invalid_argument("campaign field must be an integer");
    const auto text=value.numberLexeme(); std::uint32_t n=0;
    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),n);
    if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size())
        throw std::invalid_argument("campaign integer out of range");
    return n;
}
double number(const Object& o,const char* key) {
    const auto& value=field(o,key);
    if(!value.isNumber() || !std::isfinite(value.number())) throw std::invalid_argument("invalid campaign number");
    return value.number();
}
void size(const Object& o,const std::size_t n) {
    if(o.size()!=n) throw std::invalid_argument("unknown or missing campaign field");
}
}
CampaignContent parseCampaignContent(const json::Value& value) {
    const auto& root=value.object(); size(root,4);
    const auto& v=field(root,"vertical").object(); size(v,13);
    const auto& d=field(root,"dimensions").object(); size(d,11);
    const auto& a=field(root,"aion").object(); size(a,11);
    CampaignContent c;
    c.vertical={integer(v,"durationTicks"),integer(v,"slots"),integer(v,"costStepPermille"),
        number(v,"floorHeightMeters"),integer(v,"faithMax"),integer(v,"startingFaith"),
        integer(v,"faithGrowth"),integer(v,"faithNeighborLimit"),integer(v,"triangleFaith"),
        integer(v,"heightFaithLimit"),integer(v,"imageCost"),integer(v,"imageCooldownTicks"),
        integer(v,"imageFaith")};
    c.dimensions={integer(d,"foreignCount"),integer(d,"seedVersion"),integer(d,"initialRivalStores"),
        integer(d,"inversionCost"),integer(d,"inversionCooldownTicks"),integer(d,"energyDurationTicks"),
        integer(d,"energyRevenuePermille"),integer(d,"energyFaith"),integer(d,"maxActive"),
        integer(d,"escapeCost"),integer(d,"escapeCooldownTicks")};
    c.aion={integer(a,"warningTicks"),integer(a,"manifestationTicks"),integer(a,"spawnPeriodTicks"),
        integer(a,"maxValueWarningTicks"),integer(a,"budgetCredits"),integer(a,"buildCostCredits"),
        number(a,"zocRadiusMeters"),integer(a,"timePeriodTicks"),integer(a,"historyTicks"),
        integer(a,"pasteLotOffset"),integer(a,"pasteLimit")};
    c.skills=parseSkillContent(field(root,"skills"));
    validateCampaignContent(c); return c;
}
}
