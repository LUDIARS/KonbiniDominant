#pragma once

#include <cstdint>

#include "konbini/adapters/ergo/frame_outcome.h"

// @implements spec/interface/pictor-rendering.md Surface / device recovery
// @implements spec/interface/mobile-platform.md Lifecycle

namespace konbini::adapters::ergo {

enum class RenderLifecycleState : std::uint8_t {
    // No render device / surface attached. The game session stays alive.
    Detached = 0,
    // Frames may be recorded, submitted and presented.
    Running,
    // Paused, backgrounded or zero-area: presentation is suspended in Pictor
    // and no game frame or GPU work is issued.
    Suspended,
    // Surface lost, device lost or render failure was observed. No GPU work
    // until the host tears the render device down and initializes it again.
    ReinitializeRequired,
};

// Decides, per app-owner thread, whether the render device may submit work.
// It owns no GPU resources; the host applies `presentationSuspended()` to
// `RenderDeviceHost::setPresentationSuspended()` after every change.
class RenderLifecycle {
public:
    // A render device was initialized against a native surface. Clears a
    // previous loss: reinitialize is the only way out of it.
    void attached() noexcept;
    // The render device was torn down (surface released or reinitialize).
    void detached() noexcept;
    void setPaused(bool paused) noexcept;
    void setDrawableArea(bool hasArea) noexcept;

    // Records a frame outcome. Loss / failure moves to ReinitializeRequired
    // and is kept until `attached()`; a resize-class outcome never clears it.
    void observe(FrameOutcome outcome) noexcept;

    [[nodiscard]] RenderLifecycleState state() const noexcept;
    [[nodiscard]] bool maySubmit() const noexcept;
    [[nodiscard]] bool presentationSuspended() const noexcept;
    // The outcome that required reinitialize, or Presented when none did.
    [[nodiscard]] FrameOutcome lossCause() const noexcept;

private:
    bool attached_ = false;
    bool paused_ = false;
    bool hasArea_ = true;
    FrameOutcome loss_ = FrameOutcome::Presented;
};

[[nodiscard]] const char* describeRenderLifecycleState(
    RenderLifecycleState state) noexcept;

}  // namespace konbini::adapters::ergo
