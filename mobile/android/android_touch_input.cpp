#include "android_touch_input.h"

#include <android/input.h>

#include <cstddef>
#include <optional>

#include "konbini/app/input/touch_sample.h"
#include "konbini/app/native_mobile_runtime.h"

namespace konbini::android_host {
namespace {

using app::TouchPhase;

[[nodiscard]] std::optional<TouchPhase> phaseOf(const std::int32_t kind) noexcept {
    switch (kind) {
        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_POINTER_DOWN:
            return TouchPhase::Down;
        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_POINTER_UP:
            return TouchPhase::Up;
        case AMOTION_EVENT_ACTION_MOVE:
            return TouchPhase::Move;
        case AMOTION_EVENT_ACTION_CANCEL:
            return TouchPhase::Cancel;
        default:
            return std::nullopt;
    }
}

}  // namespace

// @implements spec/interface/mobile-platform.md Input and UI
std::int32_t forwardMotionEvent(const AInputEvent* event, app::NativeMobileRuntime& game) {
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) {
        return 0;
    }
    const std::int32_t action = AMotionEvent_getAction(event);
    const auto phase = phaseOf(action & AMOTION_EVENT_ACTION_MASK);
    if (!phase) {
        return 1;
    }
    const auto actionIndex = static_cast<std::size_t>(
        (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
    // Event time is CLOCK_MONOTONIC nanoseconds; presentation-only.
    const double seconds = static_cast<double>(AMotionEvent_getEventTime(event)) * 1e-9;
    const std::size_t count = AMotionEvent_getPointerCount(event);
    for (std::size_t index = 0; index < count; ++index) {
        // MOVE and CANCEL report every pointer; DOWN / UP only the acting one.
        if (*phase != TouchPhase::Move && *phase != TouchPhase::Cancel && index != actionIndex) {
            continue;
        }
        const auto finger = static_cast<std::uint64_t>(AMotionEvent_getPointerId(event, index));
        // Window pixels are framebuffer pixels on Android: no client scale.
        game.touch(app::normalizeTouchSample(finger, *phase, AMotionEvent_getX(event, index),
                                             AMotionEvent_getY(event, index), seconds));
    }
    return 1;
}

}  // namespace konbini::android_host
