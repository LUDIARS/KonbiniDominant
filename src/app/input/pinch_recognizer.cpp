// @implements spec/interface/mobile-platform.md Input and UI
#include "konbini/app/input/pinch_recognizer.h"

namespace konbini::app {
namespace {
// Below one pixel the ratio is noise and can explode toward infinity.
constexpr double kMinPinchDistancePixels = 1.0;
}  // namespace

void PinchRecognizer::observe(
    const ContactPair& before, const ContactPair& after) noexcept {
    if (before.count != 2 || after.count != 2 || before.first != after.first ||
        before.second != after.second) {
        return;
    }
    if (before.distancePixels > kMinPinchDistancePixels &&
        after.distancePixels > kMinPinchDistancePixels) {
        ratio_ *= after.distancePixels / before.distancePixels;
    }
}

double PinchRecognizer::consume() noexcept {
    const double ratio = ratio_;
    ratio_ = 1.0;
    return ratio;
}

void PinchRecognizer::reset() noexcept {
    ratio_ = 1.0;
}

}  // namespace konbini::app
