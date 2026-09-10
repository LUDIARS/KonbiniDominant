#include "konbini/app/playtest_pilot.h"
#include <cmath>
#include <limits>
namespace konbini::app {
using namespace sim;
PlaytestPilot::Decision PlaytestPilot::expandStores(const Context& ctx) {
    const auto& snapshot=ctx.snapshot;auto& commands=ctx.commands;
    const auto& hud=snapshot.hud();const auto chain=*hud.playerChain;const auto tick=hud.completedTicks;
    const auto occupied=[&](FacilityId id,std::uint32_t floor) {return ctx.occupied(id,floor);};
    const auto base=hud.chains[chainIndex(chain)].buildCost;
    const auto reserve=hud.phase==GamePhase::Phase1?0:3000;
    if(hud.cashCredits<base+reserve) return bt::Status::Running;
    std::size_t own=0;
    for(const auto& store:snapshot.stores()) if(store.chain==chain) ++own;
    if(own>=64) return bt::Status::Running;
    const RenderFacility* best=nullptr;double bestScore=-std::numeric_limits<double>::infinity();
    for(const auto& f:snapshot.facilities()) {
        if(!f.isBuildable || occupied(f.id,0)) continue;
        double score=0;
        for(const auto& cell:snapshot.populationCells()) {
            const auto dx=f.positionMeters.x-cell.positionMeters.x,dz=f.positionMeters.z-cell.positionMeters.z;
            score+=cell.population/(1.0+dx*dx+dz*dz);
        }
        for(const auto& store:snapshot.stores()) {
            const auto dx=f.positionMeters.x-store.positionMeters.x,dz=f.positionMeters.z-store.positionMeters.z;
            const auto distance=std::sqrt(dx*dx+dz*dz);
            score+=store.chain==chain?20.0/(1.0+std::abs(distance-25.0)):-50.0/(1.0+distance);
        }
        if(score>bestScore) {bestScore=score;best=&f;}
    }
    if(best) return commands.placeStore(chain,best->id,tick);
    return bt::Status::Running;
}
}
