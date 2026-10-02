// @implements spec/interface/mobile-platform.md Input and UI
#include "konbini/app/input/tap_recognizer.h"

namespace konbini::app {

void TapRecognizer::begin() noexcept {
    tracking_ = true;
    candidate_ = true;
}

void TapRecognizer::disqualify() noexcept {
    candidate_ = false;
}

bool TapRecognizer::release() noexcept {
    const bool tapped = tracking_ && candidate_;
    reset();
    return tapped;
}

void TapRecognizer::reset() noexcept {
    tracking_ = false;
    candidate_ = false;
}

}  // namespace konbini::app
