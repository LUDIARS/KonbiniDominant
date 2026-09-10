#include "konbini/sim/aion_system.h"
#include "konbini/sim/dimension_system.h"
#include <algorithm>
namespace konbini::sim {
// MaxValue is resolved before the last-tick survival check, never after it.
void resolveAionMaxValue(CampaignWorld w,const std::span<const DominantTriangle> triangles) {
    if(w.state.phase!=GamePhase::Boss) return;
    std::vector<std::uint32_t> collapsing;
    for(auto& d:w.state.campaign.dimensions) {
        if(d.status!=DimensionStatus::Active) continue;
        if(d.maxValueAtTick) {
            const bool stillFormed=std::ranges::any_of(triangles,[&](const auto& t){
                return t.chain==ChainId::Aion && t.dimension==d.id && t.stores==d.maxValueTriangle;});
            if(!stillFormed) {d.maxValueAtTick=0;d.maxValueTriangle={};}
        }
        if(d.maxValueAtTick && w.state.campaign.elapsedTicks>=d.maxValueAtTick) {
            collapsing.push_back(d.id); continue;
        }
        if(!d.maxValueAtTick) {
            const auto found=std::ranges::find_if(triangles,[&](const auto& t){
                return t.chain==ChainId::Aion && t.dimension==d.id;});
            if(found!=triangles.end()) {
                d.maxValueTriangle=found->stores;
                d.maxValueAtTick=w.state.campaign.elapsedTicks+w.content.campaign->aion.maxValueWarningTicks;
            }
        }
    }
    for(const auto dimension:collapsing) {
        collapseCampaignDimension(w,dimension,false); ++w.state.campaign.maxValueEvents;
    }
}
}
