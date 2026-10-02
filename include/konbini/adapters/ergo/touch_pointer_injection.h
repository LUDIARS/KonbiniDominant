#pragma once

#include <cstdint>

#include "konbini/app/pointer_sample.h"

// @implements spec/interface/mobile-platform.md Input and UI
// @implements spec/interface/ergo-runtime.md Input adapter

namespace konbini::adapters::ergo {

// What the bridge writes into Ergo's mouse device for one touch frame.
// Pinned Ergo (7f0d6bbd) has no touch device, so the primary contact is
// mirrored as pointer position + left button. Gesture meaning (tap / drag /
// pinch / cancel) stays in the game-owned recognizers; Ergo only sees a
// pointer that is never left pressed after release or cancel.
struct ErgoPointerInjection {
    float xPixels = 0.0F;
    float yPixels = 0.0F;
    // Ergo `MouseButton` bit mask to inject.
    std::uint8_t buttons = 0;
};

// `mouseButtons` is the bridge's real mouse button mask, kept so a mouse
// button held during touch is not released by the touch frame.
[[nodiscard]] ErgoPointerInjection planTouchPointerInjection(
    const app::PointerSample& touch, std::uint8_t mouseButtons) noexcept;

}  // namespace konbini::adapters::ergo
