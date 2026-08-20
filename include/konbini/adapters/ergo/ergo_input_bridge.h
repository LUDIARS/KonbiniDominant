#pragma once

#include <cstdint>

struct GLFWwindow;

namespace ergo::input {
class InputSystem;
}

// @implements spec/interface/ergo-runtime.md Input adapter

namespace konbini::adapters::ergo {

// 1 frame 分の pointer 状態。
//
// Ergo の `MouseDevice::injectPosition()` は delta を「直前の inject との差」
// で上書きするため、1 frame 内に複数の move callback が来ると最後の 1 件しか
// 残らない。frame 単位の累積が要る drag では使えないので、pointer の位置 /
// delta / scroll は bridge 側が正本を持ち、Ergo へは真値の反映として inject
// する。button / key の状態と edge 判定は Ergo の device が正本。
struct PointerFrame {
    double xPixels = 0.0;
    double yPixels = 0.0;
    double deltaXPixels = 0.0;
    double deltaYPixels = 0.0;
    double scrollSteps = 0.0;
    bool insideWindow = false;
};

// GLFW callback を Ergo の inject API へ変換する adapter。
//
// pinned Ergo (771b027f) の `ergo_input` は platform poll が no-op なので、
// device 状態は host が inject するしかない。raw callback から simulation
// state を触らず、Ergo の double buffer へ入れるだけに留める。
//
// window の user pointer は Pictor の `GlfwSurfaceProvider` が使っているので
// 奪えない。framebuffer size callback も Pictor のものを残し、この bridge は
// key / mouse button / cursor / scroll / focus / enter だけを登録する。
class ErgoInputBridge {
public:
    ErgoInputBridge() = default;
    ~ErgoInputBridge();

    ErgoInputBridge(const ErgoInputBridge&) = delete;
    ErgoInputBridge& operator=(const ErgoInputBridge&) = delete;

    // `window` と `system` は借用。どちらもこの bridge より長生きすること。
    void attach(GLFWwindow* window, ::ergo::input::InputSystem& system);
    void detach() noexcept;

    [[nodiscard]] bool isAttached() const noexcept;

    // `poll_events()` の後、action map で読む前に呼ぶ。Ergo の double buffer
    // を進め、pointer の frame 累積を確定する。
    void beginFrame();

    // frame の終わりに呼ぶ。次 frame の累積を 0 から始める。
    void endFrame() noexcept;

    [[nodiscard]] PointerFrame pointerFrame() const noexcept;

    // focus を失った frame では inject 済みの key / button を全て up にして
    // stuck key を残さない。値は 1 度だけ true を返す。
    [[nodiscard]] bool consumeFocusLost() noexcept;

    // UI capture 中は game command を抑止する。first playable の HUD は
    // 入力を取らないので既定は false だが、抑止の判断点は 1 箇所に置く。
    void setUiCapture(bool captured) noexcept;
    [[nodiscard]] bool uiCaptured() const noexcept;

private:
    static void keyCallback(
        GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouseButtonCallback(
        GLFWwindow* window, int button, int action, int mods);
    static void cursorPositionCallback(
        GLFWwindow* window, double x, double y);
    static void cursorEnterCallback(GLFWwindow* window, int entered);
    static void scrollCallback(
        GLFWwindow* window, double xOffset, double yOffset);
    static void focusCallback(GLFWwindow* window, int focused);

    [[nodiscard]] static ErgoInputBridge* fromWindow(
        GLFWwindow* window) noexcept;

    void clearInjectedState() noexcept;

    GLFWwindow* window_ = nullptr;
    ::ergo::input::InputSystem* system_ = nullptr;
    std::uint8_t buttons_ = 0;
    PointerFrame pending_;
    PointerFrame frame_;
    bool hasCursorSample_ = false;
    bool focusLost_ = false;
    bool uiCaptured_ = false;
};

}  // namespace konbini::adapters::ergo
