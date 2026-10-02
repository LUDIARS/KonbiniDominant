#pragma once

#include <optional>
#include <span>

#include "konbini/app/platform/display_metrics.h"
#include "konbini/app/platform/lifecycle_event.h"
#include "konbini/app/platform/memory_pressure.h"

// @implements spec/interface/mobile-platform.md Lifecycle

namespace konbini::app {

// What the owner has to do after one ordered batch of events. The state
// machine reports; render, checkpoint and cache owners act on it.
struct LifecycleEffects {
    // Persist the latest tick-boundary checkpoint (entered background).
    bool checkpointRequested = false;
    // Stop GPU submission now (background or surface loss).
    bool gpuSubmissionStopped = false;
    // Rebuild swapchain-dependent resources with `display`.
    bool renderRebuildRequested = false;
    // Latest metrics of the batch, for render / UI layout.
    std::optional<DisplayMetrics> display;
    // Most severe pressure of the batch.
    std::optional<MemoryPressureLevel> memoryPressure;
    // Latest thermal level of the batch.
    std::optional<ThermalLevel> thermal;
};

// App-level lifecycle on the owner thread. Applying events in queue order
// keeps "pause then surface lost" and "surface lost then pause" distinct.
//
// It never owns or resets simulation state: losing the surface or entering
// the background only stops ticks, commands and GPU work, so the
// authoritative match is intact when the surface comes back.
// @implements spec/interface/mobile-platform.md Lifecycle
class LifecycleState {
public:
    // Events carrying metrics are validated (`std::invalid_argument`).
    LifecycleEffects apply(const LifecycleEvent& event);
    LifecycleEffects apply(std::span<const LifecycleEvent> events);

    [[nodiscard]] bool paused() const noexcept;
    [[nodiscard]] bool suspended() const noexcept;
    [[nodiscard]] bool surfaceAvailable() const noexcept;

    // Fixed ticks and new game commands run only while the player can see
    // and touch the game.
    [[nodiscard]] bool advancesSimulation() const noexcept;
    [[nodiscard]] bool acceptsCommands() const noexcept;
    // GPU work needs a surface and the foreground. Pause alone keeps the
    // last frame presentable.
    [[nodiscard]] bool maySubmitGpu() const noexcept;

private:
    bool paused_ = false;
    bool suspended_ = false;
    bool surfaceAvailable_ = false;
};

}  // namespace konbini::app
