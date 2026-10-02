#pragma once

#include <cstdint>

struct AInputEvent;

namespace konbini::app {
class NativeMobileRuntime;
}

namespace konbini::android_host {

// Normalizes one `AMotionEvent` into `TouchSample`s (finger ID, phase,
// window pixels, event time) and hands them to the shared touch boundary.
// Returns 1 when the event was a motion event the game consumed.
// @implements spec/interface/mobile-platform.md Input and UI
std::int32_t forwardMotionEvent(const AInputEvent* event, app::NativeMobileRuntime& game);

}  // namespace konbini::android_host
