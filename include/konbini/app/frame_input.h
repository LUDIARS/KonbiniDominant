#pragma once

#include <optional>
#include "konbini/app/pointer_sample.h"

#include "konbini/render/viewport_extent.h"
#include "konbini/sim/chain_id.h"

// @implements spec/interface/ergo-runtime.md Input adapter
// @implements spec/plan/tasks/first-playable.md Minimal controls

namespace konbini::app {

// 1 フレーム分の操作意図。Ergo / GLFW / Vulkan の型を一切含まない game-owned
// な値で、adapter (`adapters::ergo::InputActionMap`) が device 状態から作る。
// controller 群はこの型だけを見るので、input backend を差し替えても
// gameplay 側の判定は変わらない。
struct FrameInput {
    // presentation 時間。simulation の fixed tick とは別で、camera など
    // presentation 側の補間にだけ使う。
    PointerSample pointer;
    double uiScale = 1.0;
    double pinchRatio = 1.0;
    bool buildRequested = false;
    std::optional<std::uint32_t> skillChoice;
    double dtSeconds = 0.0;
    render::ViewportExtent viewport{};

    // `1` / `2` / `3` の押下。押しっぱなしで毎フレーム command を積まない
    // よう、adapter 側が edge を取ってから入れる。
    std::optional<sim::ChainId> chainRequest;

    // pointer は viewport 内 pixel。UI capture 中は adapter が
    // `primaryClick` / `secondaryClick` を落とす。
    double cursorXPixels = 0.0;
    double cursorYPixels = 0.0;
    bool cursorInsideViewport = false;
    bool primaryClick = false;
    bool secondaryClick = false;
    bool cancel = false;
    bool toggleControls = false;
    bool retry = false;
    bool togglePause = false;
    bool floorUp = false, floorDown = false, nextFreeFloor = false, groundFloor = false;
    bool buildHeld = false, imageStrategy = false, invertStore = false;
    bool nextDimension = false, escapeDimension = false;

    // camera 操作。`WASD` は正規化済みの方向、drag は pixel delta、
    // wheel は notch 数。
    double panRight = 0.0;
    double panForward = 0.0;
    double dragDeltaXPixels = 0.0;
    double dragDeltaYPixels = 0.0;
    double zoomSteps = 0.0;

    // focus を失ったフレーム。stuck key を残さないため adapter が inject
    // 状態をクリアし、この frame の操作入力は捨てる。
    bool focusLost = false;
};

}  // namespace konbini::app
