#pragma once
#include <optional>
#include <string>
#include <vector>
#include "konbini/app/hud_text_model.h"
#include "konbini/render/hud_text_geometry.h"
namespace konbini::app {
enum class PointerPage { Main, Floors, Actions };
enum class PointerAction {
    Chain1,Chain2,Chain3,Skill1,Skill2,Skill3,Build,Cancel,ZoomOut,ZoomIn,
    Floors,Actions,Back,FloorDown,FloorUp,NextFloor,Ground,Image,World,Invert,Escape,Pause,Help,Retry
};
struct PointerRect {
    float x=0,y=0,width=0,height=0;
    bool contains(double px,double py) const noexcept {
        return px>=x && py>=y && px<x+width && py<y+height;
    }
};
struct PointerButton {
    PointerAction action{};
    PointerRect rect;
    std::string label,detail;
    bool enabled=true,repeatable=false;
};
struct PointerControls {
    std::vector<PointerButton> buttons;
    PointerRect panel, statusPanel;
    std::string heading;
    std::vector<std::string> modalLines, statusLines;
    render::HudTextStyle statusStyle;
    bool modal=false;
    float uiScale=1;
};
PointerControls buildPointerControls(const HudTextInput& input,render::ViewportExtent extent,
                                     PointerPage page,double uiScale);
const PointerButton* hitPointerControl(const PointerControls& controls,double x,double y) noexcept;
render::WorldMesh buildPointerControlMesh(const PointerControls& controls,
    render::ViewportExtent extent,std::optional<PointerAction> pressed);
}
