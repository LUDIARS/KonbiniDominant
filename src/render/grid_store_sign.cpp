// @implements spec/feature/grid-town-and-vector-ui.md Square stores
#include "grid_store_geometry.h"
#include <algorithm>
#include <cmath>
#include <string_view>
#include "konbini/render/vector_font.h"
#include "konbini/render/world_palette.h"
#include "world_box_geometry.h"

namespace konbini::render::detail {
void appendGridStoreSign(WorldMesh& mesh, const sim::RenderStore& first, const std::size_t length,
                         const StoreMarkerSpec& spec) {
    const double cell = spec.gridCellMeters;
    const double width = cell * static_cast<double>(length) - 0.32;
    const double centerX = first.positionMeters.x + cell * static_cast<double>(length-1) * 0.5;
    const double panelHeight = std::min(1.2,0.72+0.24*std::log2(static_cast<double>(length)));
    const double top = first.positionMeters.y + spec.heightMeters + 0.30;
    const double front = first.positionMeters.z + cell*0.5 - 0.10;
    const WorldColor panel{0.97F,0.92F,0.78F,spec.alpha};
    appendAxisAlignedBox(mesh,{centerX,top-panelHeight*0.5,front-0.06},{width*0.5,panelHeight*0.5,0.055},panel);
    const auto ink = chainColor(first.chain,spec.alpha);
    appendAxisAlignedBox(mesh,{centerX,top+0.015,front-0.06},{width*0.5,0.045,0.08},ink);
    const std::string_view name = first.chain == sim::ChainId::Losan ? "MOONPANTRY"
        : first.chain == sim::ChainId::Famoma ? "SUNFOLD" : "DAYLARK";
    float advance = 0;
    for (const char c : name) advance += vectorGlyph(c).advance;
    const double height = std::min(panelHeight*0.70,(width-0.5)/advance);
    double left = centerX - advance*height*0.5;
    const double textTop = top - (panelHeight-height)*0.5;
    for (const char c : name) {
        const auto glyph = vectorGlyph(c);
        const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
        for (const auto p : glyph.points)
            mesh.vertices.push_back({{static_cast<float>(left+p[0]/kVectorFontUnits*height),
                static_cast<float>(textTop-p[1]/kVectorFontUnits*height),static_cast<float>(front)},
                {0,0,1},ink});
        // Font coordinates are Y-down; the facade uses Y-up.
        for (std::size_t i = 0; i < glyph.indices.size(); i += 3)
            mesh.indices.insert(mesh.indices.end(),{base+glyph.indices[i],
                base+glyph.indices[i+2],base+glyph.indices[i+1]});
        left += glyph.advance*height;
    }
}
}
