// @implements spec/feature/grid-town-and-vector-ui.md Grid town
#include "konbini/city/grid_town.h"
#include <array>
#include <stdexcept>

namespace konbini::city {
void validateGridTownCells(const CityManifest& manifest) {
    if (manifest.facilities.size() != kGridTownSide*kGridTownSide)
        throw std::invalid_argument("grid town must contain exactly 256 ground cells");
    const double half = kGridCellMeters*0.5;
    for (std::size_t index=0;index<manifest.facilities.size();++index) {
        const auto& cell = manifest.facilities[index];
        const int x=static_cast<int>(index%kGridTownSide), z=static_cast<int>(index/kGridTownSide);
        const double px=(x+0.5-kGridTownSide*0.5)*kGridCellMeters;
        const double pz=(z+0.5-kGridTownSide*0.5)*kGridCellMeters;
        const auto& p=cell.recipe.originMeters;
        const auto& b=cell.boundsMeters;
        const std::array actual{p.x,p.y,p.z,b.min.x,b.min.y,b.min.z,b.max.x,b.max.y,b.max.z};
        const std::array expected{px,0.0,pz,px-half,0.0,pz-half,px+half,0.2,pz+half};
        if (actual!=expected || cell.cell.x!=x || cell.cell.z!=z || !cell.isBuildable || cell.recipe.kind!="grid-cell")
            throw std::invalid_argument("grid town cell footprint is inconsistent");
    }
}
}
