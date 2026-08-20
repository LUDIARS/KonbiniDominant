#include "konbini/adapters/ergo/ergo_input_bridge.h"

#include <map>
#include <optional>
#include <stdexcept>

#include <GLFW/glfw3.h>

#include "ergo/input/input_system.h"
#include "ergo/input/keyboard_device.h"
#include "ergo/input/mouse_device.h"

// @implements spec/interface/ergo-runtime.md Input adapter

namespace konbini::adapters::ergo {
namespace {

// Pictor の `GlfwSurfaceProvider` が window user pointer を占有しているので、
// bridge は自前の登録表で window から自分を引く。first playable は 1 window
// だが、表にしておけば「別 window の callback を自分のものとして処理する」
// 事故が起きない。
std::map<GLFWwindow*, ErgoInputBridge*>& bridgeRegistry() {
    static std::map<GLFWwindow*, ErgoInputBridge*> registry;
    return registry;
}

[[nodiscard]] std::optional<::ergo::input::KeyCode> toErgoKey(
    const int glfwKey) noexcept {
    using ::ergo::input::KeyCode;
    // Pinned Ergo logs injected keyboard transitions at INFO level. Inject
    // only actual first-playable controls so unrelated typing while the
    // window is focused never enters the input device or its logs.
    switch (glfwKey) {
        case GLFW_KEY_A:
            return KeyCode::A;
        case GLFW_KEY_D:
            return KeyCode::D;
        case GLFW_KEY_S:
            return KeyCode::S;
        case GLFW_KEY_W:
            return KeyCode::W;
        case GLFW_KEY_1:
            return KeyCode::Num1;
        case GLFW_KEY_2:
            return KeyCode::Num2;
        case GLFW_KEY_3:
            return KeyCode::Num3;
        case GLFW_KEY_ESCAPE:
            return KeyCode::Escape;
        case GLFW_KEY_F1:
            return KeyCode::F1;
        default:
            // 未対応の key は無視する。first playable の操作表 (WASD /
            // 1-3 / Esc / F1) を外れる入力へ意味を与えない。
            return std::nullopt;
    }
}

[[nodiscard]] std::optional<std::uint8_t> toErgoButtonBit(
    const int glfwButton) noexcept {
    switch (glfwButton) {
        case GLFW_MOUSE_BUTTON_LEFT:
            return static_cast<std::uint8_t>(
                ::ergo::input::MouseButton::Left);
        case GLFW_MOUSE_BUTTON_RIGHT:
            return static_cast<std::uint8_t>(
                ::ergo::input::MouseButton::Right);
        case GLFW_MOUSE_BUTTON_MIDDLE:
            return static_cast<std::uint8_t>(
                ::ergo::input::MouseButton::Middle);
        default:
            return std::nullopt;
    }
}

}  // namespace

ErgoInputBridge::~ErgoInputBridge() {
    detach();
}

// @implements spec/interface/ergo-runtime.md Input adapter
void ErgoInputBridge::attach(
    GLFWwindow* const window, ::ergo::input::InputSystem& system) {
    if (window_ != nullptr) {
        throw std::logic_error("input bridge is already attached");
    }
    if (window == nullptr) {
        throw std::invalid_argument("input bridge requires a window");
    }
    if (system.mouse() == nullptr || system.keyboard() == nullptr) {
        // required backend 不在で no-op module へ縮退しない
        // (ergo-runtime.md#Failure)。
        throw std::runtime_error(
            "ergo input system has no mouse or keyboard device");
    }
    if (bridgeRegistry().find(window) != bridgeRegistry().end()) {
        throw std::logic_error(
            "another input bridge is already attached to this window");
    }

    window_ = window;
    system_ = &system;
    bridgeRegistry().emplace(window, this);

    // Ergo の device は inject 前に connected でないと状態を保持しない。
    system_->mouse()->setConnected(true);
    system_->keyboard()->setConnected(true);

    glfwSetKeyCallback(window_, &ErgoInputBridge::keyCallback);
    glfwSetMouseButtonCallback(window_, &ErgoInputBridge::mouseButtonCallback);
    glfwSetCursorPosCallback(window_, &ErgoInputBridge::cursorPositionCallback);
    glfwSetCursorEnterCallback(window_, &ErgoInputBridge::cursorEnterCallback);
    glfwSetScrollCallback(window_, &ErgoInputBridge::scrollCallback);
    glfwSetWindowFocusCallback(window_, &ErgoInputBridge::focusCallback);

    double cursorX = 0.0;
    double cursorY = 0.0;
    glfwGetCursorPos(window_, &cursorX, &cursorY);
    pending_.xPixels = cursorX;
    pending_.yPixels = cursorY;
    pending_.insideWindow =
        glfwGetWindowAttrib(window_, GLFW_HOVERED) == GLFW_TRUE;
    frame_ = pending_;
    hasCursorSample_ = true;
    system_->mouse()->injectPosition(
        {static_cast<float>(cursorX), static_cast<float>(cursorY)});
}

void ErgoInputBridge::detach() noexcept {
    if (window_ == nullptr) {
        return;
    }
    glfwSetKeyCallback(window_, nullptr);
    glfwSetMouseButtonCallback(window_, nullptr);
    glfwSetCursorPosCallback(window_, nullptr);
    glfwSetCursorEnterCallback(window_, nullptr);
    glfwSetScrollCallback(window_, nullptr);
    glfwSetWindowFocusCallback(window_, nullptr);
    bridgeRegistry().erase(window_);
    clearInjectedState();
    window_ = nullptr;
    system_ = nullptr;
    hasCursorSample_ = false;
}

bool ErgoInputBridge::isAttached() const noexcept {
    return window_ != nullptr && system_ != nullptr;
}

void ErgoInputBridge::beginFrame() {
    if (!isAttached()) {
        throw std::logic_error("input bridge is not attached");
    }
    frame_ = pending_;
    // Ergo 側の scroll / delta は inject 値がそのまま残るので、frame の
    // 正本 (bridge の累積) を反映してから double buffer を進める。
    system_->mouse()->injectScroll(
        {0.0F, static_cast<float>(frame_.scrollSteps)});
    system_->beginFrame();
}

void ErgoInputBridge::endFrame() noexcept {
    pending_.deltaXPixels = 0.0;
    pending_.deltaYPixels = 0.0;
    pending_.scrollSteps = 0.0;
    if (system_ != nullptr && system_->mouse() != nullptr) {
        // 次 frame の read buffer へ古い scroll が持ち越されないようにする
        // (DoubleBuffer::swap は write buffer へ現在値を複製する)。
        system_->mouse()->injectScroll({0.0F, 0.0F});
    }
    if (system_ != nullptr) {
        system_->endFrame();
    }
}

PointerFrame ErgoInputBridge::pointerFrame() const noexcept {
    return frame_;
}

bool ErgoInputBridge::consumeFocusLost() noexcept {
    const bool lost = focusLost_;
    focusLost_ = false;
    return lost;
}

void ErgoInputBridge::setUiCapture(const bool captured) noexcept {
    uiCaptured_ = captured;
}

bool ErgoInputBridge::uiCaptured() const noexcept {
    return uiCaptured_;
}

void ErgoInputBridge::clearInjectedState() noexcept {
    if (system_ == nullptr) {
        return;
    }
    buttons_ = 0;
    if (::ergo::input::MouseDevice* const mouse = system_->mouse();
        mouse != nullptr) {
        mouse->injectButtonState(0);
        mouse->injectScroll({0.0F, 0.0F});
    }
    if (::ergo::input::KeyboardDevice* const keyboard = system_->keyboard();
        keyboard != nullptr) {
        for (int key = 0; key < static_cast<int>(
                                    ::ergo::input::KeyCode::MaxKey);
             ++key) {
            keyboard->injectKeyState(
                static_cast<::ergo::input::KeyCode>(key), false);
        }
        keyboard->clearTextInput();
    }
}

ErgoInputBridge* ErgoInputBridge::fromWindow(GLFWwindow* const window) noexcept {
    const auto entry = bridgeRegistry().find(window);
    return entry == bridgeRegistry().end() ? nullptr : entry->second;
}

void ErgoInputBridge::keyCallback(
    GLFWwindow* const window, const int key, int /*scancode*/,
    const int action, int /*mods*/) {
    ErgoInputBridge* const bridge = fromWindow(window);
    if (bridge == nullptr || bridge->system_ == nullptr ||
        action == GLFW_REPEAT) {
        return;
    }
    const std::optional<::ergo::input::KeyCode> code = toErgoKey(key);
    if (!code.has_value()) {
        return;
    }
    bridge->system_->keyboard()->injectKeyState(
        *code, action == GLFW_PRESS);
}

void ErgoInputBridge::mouseButtonCallback(
    GLFWwindow* const window, const int button, const int action,
    int /*mods*/) {
    ErgoInputBridge* const bridge = fromWindow(window);
    if (bridge == nullptr || bridge->system_ == nullptr) {
        return;
    }
    const std::optional<std::uint8_t> bit = toErgoButtonBit(button);
    if (!bit.has_value()) {
        return;
    }
    const auto mask = static_cast<std::uint8_t>(1U << *bit);
    if (action == GLFW_PRESS) {
        bridge->buttons_ = static_cast<std::uint8_t>(bridge->buttons_ | mask);
    } else if (action == GLFW_RELEASE) {
        bridge->buttons_ =
            static_cast<std::uint8_t>(bridge->buttons_ & ~mask);
    }
    bridge->system_->mouse()->injectButtonState(bridge->buttons_);
}

void ErgoInputBridge::cursorPositionCallback(
    GLFWwindow* const window, const double x, const double y) {
    ErgoInputBridge* const bridge = fromWindow(window);
    if (bridge == nullptr || bridge->system_ == nullptr) {
        return;
    }
    if (bridge->hasCursorSample_) {
        bridge->pending_.deltaXPixels += x - bridge->pending_.xPixels;
        bridge->pending_.deltaYPixels += y - bridge->pending_.yPixels;
    }
    bridge->pending_.xPixels = x;
    bridge->pending_.yPixels = y;
    bridge->hasCursorSample_ = true;
    bridge->system_->mouse()->injectPosition(
        {static_cast<float>(x), static_cast<float>(y)});
}

void ErgoInputBridge::cursorEnterCallback(
    GLFWwindow* const window, const int entered) {
    ErgoInputBridge* const bridge = fromWindow(window);
    if (bridge == nullptr) {
        return;
    }
    bridge->pending_.insideWindow = entered == GLFW_TRUE;
}

void ErgoInputBridge::scrollCallback(
    GLFWwindow* const window, double /*xOffset*/, const double yOffset) {
    ErgoInputBridge* const bridge = fromWindow(window);
    if (bridge == nullptr) {
        return;
    }
    bridge->pending_.scrollSteps += yOffset;
}

void ErgoInputBridge::focusCallback(
    GLFWwindow* const window, const int focused) {
    ErgoInputBridge* const bridge = fromWindow(window);
    if (bridge == nullptr) {
        return;
    }
    if (focused == GLFW_TRUE) {
        return;
    }
    // focus 喪失時に押下状態を残すと、戻ってきたときに key が押しっぱなしの
    // まま camera が流れる。
    bridge->clearInjectedState();
    bridge->pending_.deltaXPixels = 0.0;
    bridge->pending_.deltaYPixels = 0.0;
    bridge->pending_.scrollSteps = 0.0;
    bridge->focusLost_ = true;
}

}  // namespace konbini::adapters::ergo
