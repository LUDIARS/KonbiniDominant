// @implements spec/feature/pointer-controls.md
// @implements spec/interface/mobile-platform.md Input and UI
#include "konbini/app/pointer_input_controller.h"
namespace konbini::app {
namespace {
// Pixels a world contact may travel before it becomes a drag, per UI scale.
constexpr double kDragSlopUiPixels=10;
}
void PointerInputController::cancelGesture() noexcept {
    active_=false;uiCapture_=false;uiMoved_=false;repeated_=false;
    tap_.reset();drag_.reset();
    capturedAction_.reset();heldSeconds_=0;
}
void PointerInputController::reset() noexcept {cancelGesture();page_=PointerPage::Main;lastCancel_=GestureCancelReason::None;}
void PointerInputController::activate(const PointerAction action,FrameInput& input) {
    switch(action) {
    case PointerAction::Chain1: input.chainRequest=sim::ChainId::Losan;break;
    case PointerAction::Chain2: input.chainRequest=sim::ChainId::Famoma;break;
    case PointerAction::Chain3: input.chainRequest=sim::ChainId::SebanIleban;break;
    case PointerAction::Skill1: input.skillChoice=0;break;
    case PointerAction::Skill2: input.skillChoice=1;break;
    case PointerAction::Skill3: input.skillChoice=2;break;
    case PointerAction::Place: input.buildRequested=true;break;
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
    input.primaryClick=false;input.primaryClickFromTouch=false;
    const auto& p=input.pointer;
    const bool surfaceChanged=viewport_!=input.viewport;
    viewport_=input.viewport;
    const auto cancel=recognizeGestureCancel({.contactCancelled=p.cancelled,.focusLost=input.focusLost,
        .surfaceChanged=active_ && surfaceChanged,.contextChanged=active_ && context!=context_});
    if(cancel!=GestureCancelReason::None) {
        lastCancel_=cancel;cancelGesture();return input;
    }
    if(p.pressed) {
        cancelGesture();
        active_=true;context_=context;lastWasTouch_=p.isTouch;
        tap_.begin();
        drag_.begin(p.pressXPixels,p.pressYPixels,kDragSlopUiPixels*controls.uiScale);
        uiCapture_=controls.modal || controls.panel.contains(p.pressXPixels,p.pressYPixels) ||
            controls.statusPanel.contains(p.pressXPixels,p.pressYPixels);
        if(const auto* button=hitPointerControl(controls,p.pressXPixels,p.pressYPixels)) capturedAction_=button->action;
    }
    if(!active_) return input;
    if(!p.pressed) heldSeconds_+=input.dtSeconds;
    const auto* hovered=hitPointerControl(controls,p.xPixels,p.yPixels);
    if(uiCapture_) {
        // UI owns the entire gesture, even on blank/disabled areas and outside
        // release. Nothing below this branch can reach the world.
        if(p.multipleContacts || (capturedAction_ && (!hovered || hovered->action!=*capturedAction_)))
            uiMoved_=true;
        if(!uiMoved_ && p.down && hovered && (hovered->enabled || repeated_) && hovered->repeatable && heldSeconds_>=0.35) {
            input.buildHeld=true;repeated_=true;
        }
        if(p.released) {
            if(!uiMoved_ && !repeated_ && hovered && hovered->enabled && capturedAction_==hovered->action)
                activate(hovered->action,input);
            cancelGesture();
        }
        return input;
    }
    const auto step=drag_.update(p);
    if(step.dragging) {
        tap_.disqualify();
        if(!controls.modal) {
            input.dragDeltaXPixels+=step.deltaXPixels;
            input.dragDeltaYPixels+=step.deltaYPixels;
            input.pinchRatio=p.pinchRatio;
        }
    }
    if(p.released) {
        if(tap_.release() && input.cursorInsideViewport && !controls.panel.contains(p.xPixels,p.yPixels) &&
           !controls.statusPanel.contains(p.xPixels,p.yPixels)) {
            input.primaryClick=true;input.primaryClickFromTouch=p.isTouch;
        }
        cancelGesture();
    }
    return input;
}
}
