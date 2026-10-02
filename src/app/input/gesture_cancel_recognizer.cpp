// @implements spec/interface/mobile-platform.md Input and UI
#include "konbini/app/input/gesture_cancel_recognizer.h"

namespace konbini::app {

GestureCancelReason recognizeGestureCancel(
    const GestureCancelSignals& signals) noexcept {
    if (signals.contactCancelled) return GestureCancelReason::ContactCancelled;
    if (signals.focusLost) return GestureCancelReason::FocusLost;
    if (signals.surfaceChanged) return GestureCancelReason::SurfaceChanged;
    if (signals.contextChanged) return GestureCancelReason::ContextChanged;
    return GestureCancelReason::None;
}

}  // namespace konbini::app
