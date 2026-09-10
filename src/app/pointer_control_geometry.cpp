// @implements spec/feature/pointer-controls.md
#include "konbini/app/pointer_controls.h"
#include <algorithm>
#include <array>
namespace konbini::app {
namespace {
void quad(render::WorldMesh& mesh,const PointerRect& r,const render::WorldVertex::ColorRgba color) {
    const auto base=static_cast<std::uint32_t>(mesh.vertices.size());
    for(const auto& p:std::array<std::array<float,2>,4>{{{r.x,r.y},{r.x+r.width,r.y},
        {r.x+r.width,r.y+r.height},{r.x,r.y+r.height}}})
        mesh.vertices.push_back({{p[0],p[1],0},{},color});
    mesh.indices.insert(mesh.indices.end(),{base,base+1,base+2,base,base+2,base+3});
}
void append(render::WorldMesh& target,const render::WorldMesh& source) {
    const auto base=static_cast<std::uint32_t>(target.vertices.size());
    target.vertices.insert(target.vertices.end(),source.vertices.begin(),source.vertices.end());
    for(const auto index:source.indices) target.indices.push_back(base+index);
}
void text(render::WorldMesh& mesh,const std::string& line,const PointerRect& area,
          const float scale,const render::WorldVertex::ColorRgba color,const render::ViewportExtent extent) {
    render::HudTextStyle style;
    style.glyphPixelScale=scale;style.glyphSpacingPixels=scale/3;style.panelColor={0,0,0,0};
    const auto measured=std::max(1.0F,render::hudTextWidthPixels(line,style));
    const auto fit=std::clamp((area.width-16)/measured,0.01F,1.0F);
    style.glyphPixelScale*=fit;style.glyphSpacingPixels*=fit;
    style.originXPixels=area.x+(area.width-render::hudTextWidthPixels(line,style))/2;
    style.originYPixels=area.y+(area.height-7*style.glyphPixelScale)/2;
    style.textColor=color;
    append(mesh,render::buildHudTextMesh(std::vector<std::string>{line},style,extent));
}
}
render::WorldMesh buildPointerControlMesh(const PointerControls& controls,const render::ViewportExtent extent,
                                         const std::optional<PointerAction> pressed) {
    render::WorldMesh mesh;
    quad(mesh,controls.panel,controls.modal?render::WorldVertex::ColorRgba{0.01F,0.02F,0.045F,0.92F}:
        render::WorldVertex::ColorRgba{0.015F,0.025F,0.045F,0.95F});
    if(controls.modal && !controls.buttons.empty())
        text(mesh,controls.heading,{0,controls.buttons.front().rect.y-42*controls.uiScale,
            static_cast<float>(extent.width),34*controls.uiScale},2.5F*controls.uiScale,{0.7F,1,0.88F,1},extent);
    if(controls.modal && !controls.buttons.empty()) {
        const auto& last=controls.buttons.back().rect;
        for(std::size_t i=0;i<controls.modalLines.size();++i)
            text(mesh,controls.modalLines[i],{0,last.y+last.height+(6+20*static_cast<float>(i))*controls.uiScale,
                static_cast<float>(extent.width),20*controls.uiScale},1.7F*controls.uiScale,{0.85F,0.92F,1,1},extent);
    }
    for(const auto& button:controls.buttons) {
        const bool active=button.enabled && pressed==button.action;
        quad(mesh,button.rect,button.enabled?render::WorldVertex::ColorRgba{0.22F,0.6F,0.58F,1}:
            render::WorldVertex::ColorRgba{0.22F,0.25F,0.3F,1});
        const auto& r=button.rect;
        quad(mesh,{r.x+2,r.y+2,r.width-4,r.height-4},active?render::WorldVertex::ColorRgba{0.1F,0.38F,0.32F,1}:
            render::WorldVertex::ColorRgba{0.035F,0.09F,0.12F,1});
        const float labelHeight=button.detail.empty()?r.height:r.height*0.56F;
        const auto color=button.enabled?render::WorldVertex::ColorRgba{0.85F,1,0.96F,1}:
            render::WorldVertex::ColorRgba{0.5F,0.55F,0.6F,1};
        text(mesh,button.label,{r.x,r.y,r.width,labelHeight},1.9F*controls.uiScale,color,extent);
        if(!button.detail.empty()) text(mesh,button.detail,{r.x,r.y+labelHeight,r.width,r.height-labelHeight},
            1.5F*controls.uiScale,color,extent);
    }
    return mesh;
}
}
