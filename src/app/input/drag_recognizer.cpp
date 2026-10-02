// @implements spec/interface/mobile-platform.md Input and UI
#include "konbini/app/input/drag_recognizer.h"

#include <cmath>

namespace konbini::app {

void DragRecognizer::begin(
    const double xPixels, const double yPixels, const double slopPixels) noexcept {
    startX_ = lastX_ = xPixels;
    startY_ = lastY_ = yPixels;
    slop_ = slopPixels;
    dragging_ = false;
}

DragStep DragRecognizer::update(const PointerSample& sample) noexcept {
    const double distance =
        std::hypot(sample.xPixels - startX_, sample.yPixels - startY_);
    if (distance > slop_ || sample.multipleContacts) {
        dragging_ = true;
    }
    DragStep step{.dragging = dragging_};
    if (dragging_) {
        // With several contacts the centroid jumps when a finger lands or
        // lifts; the contact table reports only same-pair motion as delta.
        step.deltaXPixels = sample.multipleContacts ? sample.deltaXPixels
                                                    : sample.xPixels - lastX_;
        step.deltaYPixels = sample.multipleContacts ? sample.deltaYPixels
                                                    : sample.yPixels - lastY_;
    }
    lastX_ = sample.xPixels;
    lastY_ = sample.yPixels;
    return step;
}

void DragRecognizer::reset() noexcept {
    dragging_ = false;
}

}  // namespace konbini::app
