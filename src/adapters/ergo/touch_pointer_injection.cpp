// @implements spec/interface/mobile-platform.md Input and UI
#include "konbini/adapters/ergo/touch_pointer_injection.h"

#include "ergo/input/mouse_device.h"

namespace konbini::adapters::ergo {

ErgoPointerInjection planTouchPointerInjection(
    const app::PointerSample& touch, const std::uint8_t mouseButtons) noexcept {
    const auto left = static_cast<std::uint8_t>(
        1U << static_cast<std::uint8_t>(::ergo::input::MouseButton::Left));
    ErgoPointerInjection injection;
    injection.xPixels = static_cast<float>(touch.xPixels);
    injection.yPixels = static_cast<float>(touch.yPixels);
    const bool contactHeld = touch.down && !touch.cancelled;
    injection.buttons = contactHeld
        ? static_cast<std::uint8_t>(mouseButtons | left)
        : mouseButtons;
    return injection;
}

}  // namespace konbini::adapters::ergo
