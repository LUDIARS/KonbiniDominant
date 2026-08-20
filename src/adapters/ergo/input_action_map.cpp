#include "konbini/adapters/ergo/input_action_map.h"

#include <cmath>
#include <stdexcept>

#include "ergo/input/input_system.h"
#include "ergo/input/keyboard_device.h"
#include "ergo/input/mouse_device.h"
#include "konbini/adapters/ergo/ergo_input_bridge.h"

// @implements spec/interface/ergo-runtime.md Input adapter

namespace konbini::adapters::ergo {

InputActionMap::InputActionMap(InputActionBindings bindings)
    : bindings_(bindings) {}

const InputActionBindings& InputActionMap::bindings() const noexcept {
    return bindings_;
}

// @implements spec/plan/tasks/first-playable.md Minimal controls
app::FrameInput InputActionMap::sample(
    const ::ergo::input::InputSystem& system, ErgoInputBridge& bridge,
    const render::ViewportExtent viewport, const double dtSeconds) const {
    const ::ergo::input::KeyboardDevice* const keyboard = system.keyboard();
    const ::ergo::input::MouseDevice* const mouse = system.mouse();
    if (keyboard == nullptr || mouse == nullptr) {
        throw std::runtime_error(
            "input action map requires mouse and keyboard devices");
    }
    if (!std::isfinite(dtSeconds) || dtSeconds < 0.0) {
        throw std::invalid_argument(
            "input action map requires a finite non-negative delta");
    }

    app::FrameInput input;
    input.dtSeconds = dtSeconds;
    input.viewport = viewport;
    input.focusLost = bridge.consumeFocusLost();

    const PointerFrame pointer = bridge.pointerFrame();
    input.cursorXPixels = pointer.xPixels;
    input.cursorYPixels = pointer.yPixels;
    input.cursorInsideViewport =
        pointer.insideWindow && viewport.width != 0 && viewport.height != 0 &&
        pointer.xPixels >= 0.0 && pointer.yPixels >= 0.0 &&
        pointer.xPixels <= static_cast<double>(viewport.width) &&
        pointer.yPixels <= static_cast<double>(viewport.height);
    input.zoomSteps = pointer.scrollSteps;

    if (input.focusLost) {
        // focus を失った frame の入力は捨てる。edge 判定が「離した」を
        // 拾わないまま押下状態だけが残るのを防ぐ。
        return input;
    }

    using ::ergo::input::KeyCode;
    using ::ergo::input::MouseButton;

    const bool suppressed = bridge.uiCaptured();

    if (!suppressed) {
        if (keyboard->isKeyPressed(KeyCode::Num1)) {
            input.chainRequest = sim::ChainId::Losan;
        } else if (keyboard->isKeyPressed(KeyCode::Num2)) {
            input.chainRequest = sim::ChainId::Famoma;
        } else if (keyboard->isKeyPressed(KeyCode::Num3)) {
            input.chainRequest = sim::ChainId::SebanIleban;
        }

        input.primaryClick = mouse->isButtonPressed(MouseButton::Left) &&
                             input.cursorInsideViewport;
        input.secondaryClick = mouse->isButtonPressed(MouseButton::Right);
    }

    input.cancel =
        keyboard->isKeyPressed(KeyCode::Escape) || input.secondaryClick;
    input.toggleControls = keyboard->isKeyPressed(KeyCode::F1);

    if (bindings_.panWithWasd) {
        const double right =
            (keyboard->isKeyDown(KeyCode::D) ? 1.0 : 0.0) -
            (keyboard->isKeyDown(KeyCode::A) ? 1.0 : 0.0);
        const double forward =
            (keyboard->isKeyDown(KeyCode::W) ? 1.0 : 0.0) -
            (keyboard->isKeyDown(KeyCode::S) ? 1.0 : 0.0);
        // 斜め移動が速くならないよう正規化する。
        const double length = std::sqrt(right * right + forward * forward);
        if (length > 0.0) {
            input.panRight = right / length;
            input.panForward = forward / length;
        }
    }

    if (bindings_.dragWithMiddleButton &&
        mouse->isButtonDown(MouseButton::Middle)) {
        input.dragDeltaXPixels = pointer.deltaXPixels;
        input.dragDeltaYPixels = pointer.deltaYPixels;
    }

    return input;
}

}  // namespace konbini::adapters::ergo
