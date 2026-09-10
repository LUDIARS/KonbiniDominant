#include "konbini/sim/dimension_system.h"
#include <vector>
namespace konbini::sim {
// Finishing a foreign garrison by combat must not remove every inversion target
// and leave the campaign waiting forever.
void resolveForeignConquest(CampaignWorld w) {
    if(w.state.phase!=GamePhase::Phase3 || !w.state.playerChain) return;
    std::vector<std::uint32_t> conquered;
    for(const auto& dimension:w.state.campaign.dimensions) {
        if(!dimension.foreignObjective || dimension.status!=DimensionStatus::Active) continue;
        bool player=false, rival=false;
        for(std::size_t i=0;i<w.stores.size();++i) {
            const auto store=w.stores.row(i);
            if(!store.isActive || store.dimension!=dimension.id) continue;
            if(store.chain==*w.state.playerChain) player=true;
            else rival=true;
        }
        if(player && !rival) conquered.push_back(dimension.id);
    }
    for(const auto id:conquered) {
        collapseCampaignDimension(w,id,true);
        for(auto& dimension:w.state.campaign.dimensions)
            if(dimension.id==0 && dimension.status==DimensionStatus::Active)
                dimension.energyEndTick=w.state.campaign.elapsedTicks+w.content.campaign->dimensions.energyDurationTicks;
    }
}
}
