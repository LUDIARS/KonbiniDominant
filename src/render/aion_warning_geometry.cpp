#include "konbini/render/aion_warning_geometry.h"
#include <algorithm>
#include <cmath>
namespace konbini::render {
// @implements spec/feature/full-campaign-baseline.md Controls and validation
WorldMesh buildAionWarningGeometry(const sim::HudViewModel& hud,const ViewportExtent extent) {
    if(!hud.campaign.enabled || hud.phase!=sim::GamePhase::BossWarning) return {};
    const auto seconds=(hud.campaign.phaseRemaining+hud.ticksPerSecond-1)/hud.ticksPerSecond;
    const std::vector<std::string> lines{"W A R N I N G","AION IS APPROACHING",
        "MANIFESTATION IN "+std::to_string(seconds),"ACTIONS - ESCAPE"};
    HudTextStyle style;
    style.glyphPixelScale=std::max(0.5F,std::min({7.0F,static_cast<float>(extent.width)/150.0F,
        static_cast<float>(extent.height)/50.0F}));
    style.linePaddingPixels=style.glyphPixelScale*2.0F;
    style.panelPaddingPixels=style.glyphPixelScale*2.0F;
    float width=0;
    for(const auto& line:lines) width=std::max(width,hudTextWidthPixels(line,style));
    style.originXPixels=(static_cast<float>(extent.width)-width)/2.0F;
    style.originYPixels=(static_cast<float>(extent.height)-hudLineHeightPixels(style)*4.0F)/2.0F;
    // Slow luminance breathing; no flashing full-screen strobe.
    const float pulse=0.82F+0.18F*static_cast<float>(std::sin(hud.phaseTicks*0.15));
    style.textColor={1.0F,0.32F+0.18F*pulse,0.08F,1.0F};
    style.panelColor={0.12F,0.015F,0.025F,0.96F};
    return buildHudTextMesh(lines,style,extent);
}
}  // namespace konbini::render
