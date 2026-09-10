#pragma once
#include "konbini/app/frame_input.h"
#include "konbini/app/pointer_controls.h"
namespace konbini::app {
class PointerInputController {
public:
    FrameInput translate(FrameInput input,const PointerControls& controls,std::uint64_t context);
    PointerPage page() const noexcept {return page_;}
    std::optional<PointerAction> pressed() const noexcept {return capturedAction_;}
    void cancelGesture() noexcept;
    void reset() noexcept;
private:
    void activate(PointerAction action,FrameInput& input);
    PointerPage page_=PointerPage::Main;
    std::optional<PointerAction> capturedAction_;
    bool active_=false,uiCapture_=false,dragging_=false,repeated_=false;
    double startX_=0,startY_=0,lastX_=0,lastY_=0,heldSeconds_=0;
    std::uint64_t context_=0;
};
}
