#include "konbini/render/skill_pulse_geometry.h"
#include <cmath>
namespace konbini::render {
WorldMesh buildSkillPulseGeometry(const sim::RenderSnapshot& snapshot,
                                  const std::span<const sim::RenderStore> visibleStores) {
    WorldMesh mesh;
    const auto& hud=snapshot.hud();
    const auto& c=hud.campaign;
    if(!c.enabled || !hud.playerChain) return mesh;
    const bool wave=c.skills.waveEndTick>c.elapsedTicks;
    const bool snacks=c.skills.snacksEndTick>c.elapsedTicks;
    if(!wave && !snacks) return mesh;
    const WorldVertex::ColorRgba color=wave ? WorldVertex::ColorRgba{0.8F,0.2F,1.0F,0.85F} :
        WorldVertex::ColorRgba{1.0F,0.6F,0.1F,0.85F};
    const double fraction=0.2+0.8*static_cast<double>(c.elapsedTicks%10)/10;
    for(const auto& store:visibleStores) {
        if(store.chain!=*hud.playerChain) continue;
        const double radius=store.zocRadiusMeters*fraction;
        for(unsigned segment=0;segment<32;++segment) {
            const auto base=static_cast<std::uint32_t>(mesh.vertices.size());
            for(unsigned corner=0;corner<4;++corner) {
                const double angle=6.28318530717958647692*(segment+(corner>=2?1:0))/32;
                const double r=radius+(corner==1 || corner==2 ? 0.6 : 0);
                mesh.vertices.push_back({{
                    static_cast<float>(store.positionMeters.x+std::cos(angle)*r),
                    static_cast<float>(store.positionMeters.y+0.4),
                    static_cast<float>(store.positionMeters.z+std::sin(angle)*r)}, {0,1,0}, color});
            }
            mesh.indices.insert(mesh.indices.end(),{base,base+2,base+1,base,base+3,base+2});
        }
    }
    return mesh;
}
}
