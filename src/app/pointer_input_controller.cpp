// @implements spec/feature/pointer-controls.md
#include "konbini/app/pointer_input_controller.h"
#include <cmath>
namespace konbini::app {
void PointerInputController::cancelGesture() noexcept {
    active_=false;uiCapture_=false;dragging_=false;repeated_=false;
    capturedAction_.reset();heldSeconds_=0;
}
void PointerInputController::reset() noexcept {cancelGesture();page_=PointerPage::Main;}
void PointerInputController::activate(const PointerAction action,FrameInput& input) {
    switch(action) {
    case PointerAction::Chain1: input.chainRequest=sim::ChainId::Losan;break;
    case PointerAction::Chain2: input.chainRequest=sim::ChainId::Famoma;break;
    case PointerAction::Chain3: input.chainRequest=sim::ChainId::SebanIleban;break;
    case PointerAction::Skill1: input.skillChoice=0;break;
    case PointerAction::Skill2: input.skillChoice=1;break;
    case PointerAction::Skill3: input.skillChoice=2;break;
    case PointerAction::Build: input.buildRequested=true;break;
    case PointerAction::Cancel: input.cancel=true;break;
    case PointerAction::ZoomOut: input.zoomSteps-=1;break;
    case PointerAction::ZoomIn: input.zoomSteps+=1;break;
    case PointerAction::Floors: page_=PointerPage::Floors;break;
    case PointerAction::Actions: page_=PointerPage::Actions;break;
    case PointerAction::Back: page_=PointerPage::Main;break;
    case PointerAction::FloorDown: input.floorDown=true;break;
    case PointerAction::FloorUp: input.floorUp=true;break;
    case PointerAction::NextFloor: input.nextFreeFloor=true;page_=PointerPage::Main;break;
    case PointerAction::Ground: input.groundFloor=true;page_=PointerPage::Main;break;
    case PointerAction::Image: input.imageStrategy=true;break;
    case PointerAction::World: input.nextDimension=true;page_=PointerPage::Main;break;
    case PointerAction::Invert: input.invertStore=true;break;
    case PointerAction::Escape: input.escapeDimension=true;page_=PointerPage::Main;break;
    case PointerAction::Pause: input.togglePause=true;break;
    case PointerAction::Help: input.toggleControls=true;break;
    case PointerAction::Retry: input.retry=true;page_=PointerPage::Main;break;
    }
}
FrameInput PointerInputController::translate(FrameInput input,const PointerControls& controls,const std::uint64_t context) {
    input.primaryClick=false;
    const auto& p=input.pointer;
    if(input.focusLost || p.cancelled || (active_ && context!=context_)) {
        cancelGesture();return input;
    }
    if(p.pressed) {
        cancelGesture();
        active_=true;context_=context;
        startX_=lastX_=p.pressXPixels;startY_=lastY_=p.pressYPixels;
        uiCapture_=controls.modal || controls.panel.contains(startX_,startY_) || controls.statusPanel.contains(startX_,startY_);
        if(const auto* button=hitPointerControl(controls,startX_,startY_)) capturedAction_=button->action;
    }
    if(!active_) return input;
    if(!p.pressed) heldSeconds_+=input.dtSeconds;
    const auto* hovered=hitPointerControl(controls,p.xPixels,p.yPixels);
    if(uiCapture_) {
        // UI owns the entire gesture, even on blank/disabled areas and outside release.
        if(p.multipleContacts || (capturedAction_ && (!hovered || hovered->action!=*capturedAction_)))
            dragging_=true;
        if(!dragging_ && p.down && hovered && (hovered->enabled || repeated_) && hovered->repeatable && heldSeconds_>=0.35) {
            input.buildHeld=true;repeated_=true;
        }
        if(p.released) {
            if(!dragging_ && !repeated_ && hovered && hovered->enabled && capturedAction_==hovered->action)
                activate(hovered->action,input);
            cancelGesture();
        }
        return input;
    }
    const auto distance=std::hypot(p.xPixels-startX_,p.yPixels-startY_);
    if(distance>10*controls.uiScale || p.multipleContacts) dragging_=true;
    if(dragging_ && !controls.modal) {
        input.dragDeltaXPixels+=p.multipleContacts?p.deltaXPixels:p.xPixels-lastX_;
        input.dragDeltaYPixels+=p.multipleContacts?p.deltaYPixels:p.yPixels-lastY_;
        input.pinchRatio=p.pinchRatio;
    }
    lastX_=p.xPixels;lastY_=p.yPixels;
    if(p.released) {
        if(!dragging_ && input.cursorInsideViewport && !controls.panel.contains(p.xPixels,p.yPixels) &&
           !controls.statusPanel.contains(p.xPixels,p.yPixels))
            input.primaryClick=true;
        cancelGesture();
    }
    return input;
}
}
