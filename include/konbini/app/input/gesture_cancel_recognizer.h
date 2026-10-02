#pragma once

#include <cstdint>

// @implements spec/interface/mobile-platform.md Input and UI

namespace konbini::app {

enum class GestureCancelReason : std::uint8_t {
    None,
    // The OS cancelled the contact (system gesture, incoming call, ...).
    ContactCancelled,
    // Window focus loss or app pause / background.
    FocusLost,
    // Surface resize / orientation change moved every UI rect.
    SurfaceChanged,
    // Phase, skill offer or visible dimension changed under the gesture.
    ContextChanged,
};

struct GestureCancelSignals {
    bool contactCancelled = false;
    bool focusLost = false;
    bool surfaceChanged = false;
    bool contextChanged = false;
};

// The one place that decides when an in-flight gesture must be dropped. A
// cancelled gesture never activates a button, never taps and leaves no
// pressed pointer behind.
[[nodiscard]] GestureCancelReason recognizeGestureCancel(
    const GestureCancelSignals& signals) noexcept;

}  // namespace konbini::app
