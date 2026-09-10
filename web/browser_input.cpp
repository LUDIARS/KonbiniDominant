#include "browser_input.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace konbini::web {
void BrowserInput::pointer(int id, int phase, double x, double y, bool touch) {
    if (id < 0 || phase < 0 || phase > 3 || !std::isfinite(x) || !std::isfinite(y))
        throw std::invalid_argument("invalid browser pointer");
    touch_ = touch;
    contacts_.update(static_cast<std::uint64_t>(id), static_cast<app::TouchPhase>(phase),
                     std::clamp(x, 0.0, 1280.0), std::clamp(y, 0.0, 720.0));
}
void BrowserInput::wheel(double steps) {
    if (std::isfinite(steps)) wheel_ = std::clamp(wheel_ + steps, -10.0, 10.0);
}
void BrowserInput::cancel() noexcept { contacts_.cancel(); wheel_ = 0; }
app::FrameInput BrowserInput::consume(double dt) {
    app::FrameInput input;
    input.dtSeconds = dt;
    input.viewport = {1280, 720};
    input.pointer = contacts_.consume();
    input.pointer.isTouch = touch_;
    input.cursorXPixels = input.pointer.xPixels;
    input.cursorYPixels = input.pointer.yPixels;
    input.cursorInsideViewport = true;
    input.zoomSteps = wheel_;
    wheel_ = 0;
    return input;
}
}
