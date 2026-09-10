#include "konbini/sim/faith_system.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>
namespace konbini::sim {
// A spatial cell owns a contiguous list, not a per-store heap object.
void updateCampaignFaith(CampaignWorld w,const std::span<const DominantTriangle> triangles) {
    const auto& rules=w.content.campaign->vertical;
    using Key=std::tuple<std::uint32_t,int,int>;
    std::map<Key,std::vector<StoreRow>> grid;
    for(std::size_t i=0;i<w.stores.size();++i) {
        const auto s=w.stores.row(i);
        if(s.isActive) grid[{s.dimension,static_cast<int>(std::floor(s.positionMeters.x/32)),
            static_cast<int>(std::floor(s.positionMeters.z/32))}].push_back(s);
    }
    std::vector<std::uint32_t> faith(w.stores.size());
    for(std::size_t i=0;i<w.stores.size();++i) {
        const auto s=w.stores.row(i); faith[i]=s.faith;
        if(!s.isActive || s.isAntiStore || s.chain==ChainId::Aion) continue;
        int support=0,pressure=0,height=0;
        const auto x=static_cast<int>(std::floor(s.positionMeters.x/32));
        const auto z=static_cast<int>(std::floor(s.positionMeters.z/32));
        const auto radius=static_cast<int>(std::ceil(s.zocRadiusMeters/32));
        for(int dx=-radius;dx<=radius;++dx) for(int dz=-radius;dz<=radius;++dz) {
            const auto cell=grid.find({s.dimension,x+dx,z+dz});
            if(cell==grid.end()) continue;
            for(const auto& other:cell->second) {
                const auto px=other.positionMeters.x-s.positionMeters.x,pz=other.positionMeters.z-s.positionMeters.z;
                if(other.id==s.id || px*px+pz*pz>s.zocRadiusMeters*s.zocRadiusMeters) continue;
                if(other.chain==s.chain) ++support;
                else {
                    ++pressure;
                    if(other.verticalSlot<s.verticalSlot)
                        height=std::max(height,static_cast<int>(s.verticalSlot-other.verticalSlot));
                }
            }
        }
        int delta=static_cast<int>(rules.faithGrowth)+std::min(support,static_cast<int>(rules.faithNeighborLimit))
            -std::min(pressure,static_cast<int>(rules.faithNeighborLimit))
            +std::min(height,static_cast<int>(rules.heightFaithLimit));
        if(std::ranges::any_of(triangles,[&](const auto& t){
            return t.dimension==s.dimension && t.chain==s.chain && triangleContains(t,s.positionMeters);}))
            delta+=static_cast<int>(rules.triangleFaith);
        if(w.state.playerChain && s.chain==*w.state.playerChain) {
            const auto& skills=w.state.campaign.skills;
            const auto& config=w.content.campaign->skills;
            delta+=static_cast<int>(skillRank(skills,SkillId::MultilingualStaff)*config.staffFaith)
                -static_cast<int>(skillRank(skills,SkillId::RaisedBottom)*config.raisedFaithPenalty);
        }
        for(const auto& dimension:w.state.campaign.dimensions)
            if(dimension.id==s.dimension && dimension.energyEndTick>w.state.campaign.elapsedTicks)
                delta+=static_cast<int>(w.content.campaign->dimensions.energyFaith);
        faith[i]=static_cast<std::uint32_t>(std::clamp(static_cast<int>(s.faith)+delta,0,static_cast<int>(rules.faithMax)));
    }
    for(std::size_t i=0;i<faith.size();++i) w.stores.setFaith(i,faith[i]);
}
void applyConvenienceRevenue(CampaignWorld w) {
    for(std::size_t i=0;i<w.stores.size();++i) {
        const auto store=w.stores.row(i);
        if(!store.isActive || !w.state.playerChain || store.chain!=*w.state.playerChain) continue;
        for(const auto& d:w.state.campaign.dimensions) {
            if(d.id==store.dimension && d.energyEndTick>w.state.campaign.elapsedTicks)
                w.stores.setRevenuePermille(i,std::min<std::uint32_t>(10000,
                    store.revenuePermille*w.content.campaign->dimensions.energyRevenuePermille/1000));
        }
    }
}
}
