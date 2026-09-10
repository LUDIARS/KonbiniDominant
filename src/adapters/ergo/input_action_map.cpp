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
    const double sx=viewport.width/pointer.clientWidth, sy=viewport.height/pointer.clientHeight;
    input.pointer=pointer;
    input.pointer.xPixels*=sx;input.pointer.yPixels*=sy;
    input.pointer.pressXPixels*=sx;input.pointer.pressYPixels*=sy;
    input.pointer.deltaXPixels*=sx;input.pointer.deltaYPixels*=sy;
    input.uiScale=pointer.uiScale;
    input.cursorXPixels = input.pointer.xPixels;
    input.cursorYPixels = input.pointer.yPixels;
    input.cursorInsideViewport =
        pointer.insideWindow && viewport.width != 0 && viewport.height != 0 &&
        input.cursorXPixels >= 0.0 && input.cursorYPixels >= 0.0 &&
        input.cursorXPixels < static_cast<double>(viewport.width) &&
        input.cursorYPixels < static_cast<double>(viewport.height);
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

        // Tap/click is resolved on release by the shared pointer controller.
        input.secondaryClick = mouse->isButtonPressed(MouseButton::Right);
    }

    if(suppressed) input.pointer={};
    input.cancel =
        keyboard->isKeyPressed(KeyCode::Escape) || input.secondaryClick;
    input.toggleControls = keyboard->isKeyPressed(KeyCode::F1);
    input.retry = !suppressed && keyboard->isKeyPressed(KeyCode::R);
    input.togglePause = !suppressed && keyboard->isKeyPressed(KeyCode::P);
    input.floorUp = !suppressed && keyboard->isKeyPressed(KeyCode::E);
    input.floorDown = !suppressed && keyboard->isKeyPressed(KeyCode::Q);
    input.nextFreeFloor = !suppressed && keyboard->isKeyPressed(KeyCode::F);
    input.groundFloor = !suppressed && keyboard->isKeyPressed(KeyCode::G);
    input.buildHeld = !suppressed && keyboard->isKeyDown(KeyCode::Space);
    input.imageStrategy = !suppressed && keyboard->isKeyPressed(KeyCode::C);
    input.invertStore = !suppressed && keyboard->isKeyPressed(KeyCode::I);
    input.nextDimension = !suppressed && keyboard->isKeyPressed(KeyCode::Tab);
    input.escapeDimension = !suppressed && keyboard->isKeyPressed(KeyCode::X);

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
        input.dragDeltaXPixels = input.pointer.deltaXPixels;
        input.dragDeltaYPixels = input.pointer.deltaYPixels;
    }

    return input;
}

}  // namespace konbini::adapters::ergo
