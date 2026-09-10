// @implements spec/feature/grid-town-and-vector-ui.md Square stores
#include "grid_store_geometry.h"
#include <algorithm>
#include "konbini/render/grid_store_connections.h"
#include "konbini/render/world_palette.h"
#include "world_box_geometry.h"

namespace konbini::render::detail {
namespace {
void appendTile(WorldMesh& mesh, const sim::RenderStore& store,
                const GridStoreLinks& links, const StoreMarkerSpec& spec) {
    const double half = spec.gridCellMeters*0.5;
    const double inset = 0.22;
    const double x0 = -half+(links[Left]?0:inset), x1 = half-(links[Right]?0:inset);
    const double z0 = -half+(links[Back]?0:inset), z1 = half-(links[Front]?0:inset);
    const auto p = store.positionMeters;
    const auto primary = store.isAntiStore ? WorldColor{0.92F,0.12F,0.85F,spec.alpha}
                                           : chainColor(store.chain,spec.alpha);
    const WorldColor wall{0.91F,0.87F,0.75F,spec.alpha}, glass{0.18F,0.38F,0.40F,spec.alpha};
    const auto box = [&](double x,double y,double z,double w,double h,double d,WorldColor color) {
        appendAxisAlignedBox(mesh,{p.x+x,p.y+y,p.z+z},{w*0.5,h*0.5,d*0.5},color);
    };
    const double cx=(x0+x1)*0.5,cz=(z0+z1)*0.5,w=x1-x0,d=z1-z0;
    box(cx,spec.heightMeters*0.45,cz,w,spec.heightMeters*0.90,d,wall);
    box(cx,spec.heightMeters*0.96,cz,w,spec.heightMeters*0.08,d,primary);
    if (!links[Front]) {
        box(cx,spec.heightMeters*0.43,z1+0.012,w*0.86,spec.heightMeters*0.55,0.03,glass);
        box(cx,spec.heightMeters*0.40,z1+0.04,0.10,spec.heightMeters*0.72,0.025,wall);
        box(cx,spec.heightMeters*0.08,z1+0.04,w*0.92,0.14,0.035,primary);
    }
    if (!links[Left]) box(x0-0.006,spec.heightMeters*0.42,cz,0.018,spec.heightMeters*0.45,d*0.65,glass);
    if (!links[Right]) box(x1+0.006,spec.heightMeters*0.42,cz,0.018,spec.heightMeters*0.45,d*0.65,glass);
    if (store.faith==100 || store.chain==sim::ChainId::Aion)
        box(0,spec.heightMeters+0.13,0,half*0.55,0.24,half*0.55,
            store.chain==sim::ChainId::Aion?WorldColor{0.9F,0.6F,1,spec.alpha}:WorldColor{1,0.83F,0.2F,spec.alpha});
}
}
WorldMesh buildGridStoreGeometry(std::span<const sim::RenderStore> stores, const StoreMarkerSpec& spec) {
    const auto links = connectGridStores(stores,spec.gridCellMeters);
    WorldMesh mesh;
    for (std::size_t i=0;i<stores.size();++i) appendTile(mesh,stores[i],links[i],spec);
    for (std::size_t i=0;i<stores.size();++i) {
        if (stores[i].isAntiStore || stores[i].chain==sim::ChainId::Aion || links[i][Front]) continue;
        if (links[i][Left] && !links[*links[i][Left]][Front]) continue;
        std::size_t length=1,current=i;
        while (links[current][Right] && !links[*links[current][Right]][Front]) {
            current=*links[current][Right];
            ++length;
        }
        appendGridStoreSign(mesh,stores[i],length,spec);
    }
    return mesh;
}
}
