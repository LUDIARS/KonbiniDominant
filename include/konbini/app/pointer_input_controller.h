#pragma once
#include "konbini/app/frame_input.h"
#include "konbini/app/input/drag_recognizer.h"
#include "konbini/app/input/gesture_cancel_recognizer.h"
#include "konbini/app/input/tap_recognizer.h"
#include "konbini/app/pointer_controls.h"
// @implements spec/feature/pointer-controls.md
// @implements spec/interface/mobile-platform.md Input and UI
namespace konbini::app {
// Routes one pointer / touch gesture either to the UI (button capture) or
// to the world (tap = select, drag = pan, pinch = zoom). Gesture shape is
// decided by the tap / drag / cancel recognizers; this class only owns UI
// capture and button activation.
class PointerInputController {
public:
    FrameInput translate(FrameInput input,const PointerControls& controls,std::uint64_t context);
    PointerPage page() const noexcept {return page_;}
    std::optional<PointerAction> pressed() const noexcept {return capturedAction_;}
    // True while a gesture is in flight. False after every cancel.
    bool gestureActive() const noexcept {return active_;}
    // Last cancel decision, for diagnostics and tests.
    GestureCancelReason lastCancel() const noexcept {return lastCancel_;}
    // Whether the most recent gesture came from touch. Touch taps select
    // only; placement needs the explicit Place action.
    bool lastGestureWasTouch() const noexcept {return lastWasTouch_;}
    void cancelGesture() noexcept;
    void reset() noexcept;
private:
    void activate(PointerAction action,FrameInput& input);
    PointerPage page_=PointerPage::Main;
    std::optional<PointerAction> capturedAction_;
    TapRecognizer tap_;
    DragRecognizer drag_;
    GestureCancelReason lastCancel_=GestureCancelReason::None;
    bool active_=false,uiCapture_=false,uiMoved_=false,repeated_=false,lastWasTouch_=false;
    double heldSeconds_=0;
    std::uint64_t context_=0;
    render::ViewportExtent viewport_{};
};
}
