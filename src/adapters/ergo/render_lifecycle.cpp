#include "konbini/adapters/ergo/render_lifecycle.h"

// @implements spec/interface/pictor-rendering.md Surface / device recovery

namespace konbini::adapters::ergo {

void RenderLifecycle::attached() noexcept {
    attached_ = true;
    loss_ = FrameOutcome::Presented;
}

void RenderLifecycle::detached() noexcept {
    attached_ = false;
}

void RenderLifecycle::setPaused(const bool paused) noexcept {
    paused_ = paused;
}

void RenderLifecycle::setDrawableArea(const bool hasArea) noexcept {
    hasArea_ = hasArea;
}

// @implements spec/interface/pictor-rendering.md Surface / device recovery
void RenderLifecycle::observe(const FrameOutcome outcome) noexcept {
    if (requiresReinitialize(outcome) &&
        !requiresReinitialize(loss_)) {
        // Keep the first cause: later frames only repeat Pictor's latched
        // status and would hide what actually failed.
        loss_ = outcome;
    }
}

RenderLifecycleState RenderLifecycle::state() const noexcept {
    if (!attached_) {
        return RenderLifecycleState::Detached;
    }
    if (requiresReinitialize(loss_)) {
        return RenderLifecycleState::ReinitializeRequired;
    }
    if (paused_ || !hasArea_) {
        return RenderLifecycleState::Suspended;
    }
    return RenderLifecycleState::Running;
}

bool RenderLifecycle::maySubmit() const noexcept {
    return state() == RenderLifecycleState::Running;
}

bool RenderLifecycle::presentationSuspended() const noexcept {
    return !maySubmit();
}

FrameOutcome RenderLifecycle::lossCause() const noexcept {
    return loss_;
}

const char* describeRenderLifecycleState(
    const RenderLifecycleState state) noexcept {
    switch (state) {
        case RenderLifecycleState::Detached:
            return "detached";
        case RenderLifecycleState::Running:
            return "running";
        case RenderLifecycleState::Suspended:
            return "suspended";
        case RenderLifecycleState::ReinitializeRequired:
            return "reinitialize required";
    }
    return "unknown render lifecycle state";
}

}  // namespace konbini::adapters::ergo
