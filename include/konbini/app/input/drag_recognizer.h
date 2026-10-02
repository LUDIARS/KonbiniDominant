#pragma once

#include "konbini/app/pointer_sample.h"

// @implements spec/interface/mobile-platform.md Input and UI

namespace konbini::app {

struct DragStep {
    // Latched: once the slop is crossed the sequence stays a drag.
    bool dragging = false;
    double deltaXPixels = 0.0;
    double deltaYPixels = 0.0;
};

// Turns one contact sequence into camera pan deltas once it moves farther
// than the slop from its press position, or as soon as a second contact
// joins. Pinch scale belongs to `PinchRecognizer`.
class DragRecognizer {
public:
    void begin(double xPixels, double yPixels, double slopPixels) noexcept;
    // Per frame, with the merged contact state of that frame.
    [[nodiscard]] DragStep update(const PointerSample& sample) noexcept;
    void reset() noexcept;

    [[nodiscard]] bool dragging() const noexcept { return dragging_; }

private:
    double startX_ = 0.0, startY_ = 0.0;
    double lastX_ = 0.0, lastY_ = 0.0;
    double slop_ = 0.0;
    bool dragging_ = false;
};

}  // namespace konbini::app
