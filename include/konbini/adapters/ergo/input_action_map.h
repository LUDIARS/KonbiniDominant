#pragma once

#include "konbini/app/frame_input.h"
#include "konbini/render/viewport_extent.h"

namespace ergo::input {
class InputSystem;
}

// @implements spec/interface/ergo-runtime.md Input adapter
// @implements spec/plan/tasks/first-playable.md Minimal controls

namespace konbini::adapters::ergo {

class ErgoInputBridge;

// 操作の割り当て表。remap を 1 箇所へ集めるための値で、gameplay 判定は
// 持たない。
struct InputActionBindings {
    // camera を掴んで動かす button。左は選択、右は選択解除に割り当てるので
    // 中ボタンを使う。
    bool dragWithMiddleButton = true;
    bool panWithWasd = true;
};

// Ergo device 状態 + bridge の pointer 累積を game-owned な `FrameInput` へ
// 変換する。ここから先 (controller / command) は Ergo にも GLFW にも依存
// しない。
class InputActionMap {
public:
    explicit InputActionMap(InputActionBindings bindings = {});

    [[nodiscard]] app::FrameInput sample(
        const ::ergo::input::InputSystem& system, ErgoInputBridge& bridge,
        render::ViewportExtent viewport, double dtSeconds) const;

    [[nodiscard]] const InputActionBindings& bindings() const noexcept;

private:
    InputActionBindings bindings_;
};

}  // namespace konbini::adapters::ergo
