#pragma once

#include <cstdint>

// @implements spec/interface/mobile-platform.md Input and UI

namespace konbini::app {

enum class TouchPhase : std::uint8_t { Down, Move, Up, Cancel };

// One native contact event after the platform adapter converted it to
// framebuffer pixels and seconds. Game-owned: no OS, Ergo or GLFW type.
// The finger ID, phase, position and timestamp of the native event survive
// unchanged so the gesture recognizers see exactly what the OS reported.
struct TouchSample {
    std::uint64_t fingerId = 0;
    TouchPhase phase = TouchPhase::Down;
    double xPixels = 0.0;
    double yPixels = 0.0;
    // Platform monotonic clock. Presentation-only: it orders contact events
    // and never reaches the simulation (spec/interface/mobile-platform.md
    // Determinism).
    double timestampSeconds = 0.0;
};

// Converts a native contact in client units into a `TouchSample` in
// framebuffer pixels. `pixelsPerClientX/Y` is framebuffer extent / client
// extent. Throws `std::invalid_argument` for a non-finite position or
// timestamp, a negative timestamp, or a non-positive scale. A `Cancel`
// event keeps its finger ID but its position is not validated: some hosts
// report cancel without a position.
[[nodiscard]] TouchSample normalizeTouchSample(
    std::uint64_t fingerId, TouchPhase phase, double clientX, double clientY,
    double timestampSeconds, double pixelsPerClientX = 1.0,
    double pixelsPerClientY = 1.0);

}  // namespace konbini::app
