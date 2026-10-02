// @implements spec/interface/mobile-platform.md Input and UI
#include "konbini/app/input/touch_sample.h"

#include <cmath>
#include <stdexcept>

namespace konbini::app {

TouchSample normalizeTouchSample(
    const std::uint64_t fingerId, const TouchPhase phase, const double clientX,
    const double clientY, const double timestampSeconds,
    const double pixelsPerClientX, const double pixelsPerClientY) {
    if (!std::isfinite(timestampSeconds) || timestampSeconds < 0.0) {
        throw std::invalid_argument("touch timestamp must be finite and non-negative");
    }
    if (!std::isfinite(pixelsPerClientX) || !std::isfinite(pixelsPerClientY) ||
        pixelsPerClientX <= 0.0 || pixelsPerClientY <= 0.0) {
        throw std::invalid_argument("touch client scale must be finite and positive");
    }
    TouchSample sample{.fingerId = fingerId, .phase = phase,
                       .timestampSeconds = timestampSeconds};
    if (phase == TouchPhase::Cancel) {
        return sample;
    }
    if (!std::isfinite(clientX) || !std::isfinite(clientY)) {
        throw std::invalid_argument("non-finite touch position");
    }
    sample.xPixels = clientX * pixelsPerClientX;
    sample.yPixels = clientY * pixelsPerClientY;
    return sample;
}

}  // namespace konbini::app
