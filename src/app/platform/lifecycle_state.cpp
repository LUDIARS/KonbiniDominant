#include "konbini/app/platform/lifecycle_state.h"

#include <algorithm>
#include <stdexcept>

// @implements spec/interface/mobile-platform.md Lifecycle

namespace konbini::app {
namespace {

[[nodiscard]] const DisplayMetrics& requireMetrics(const LifecycleEvent& event) {
    if (!event.display) {
        throw std::invalid_argument("lifecycle surface / display event needs display metrics");
    }
    validateDisplayMetrics(*event.display);
    return *event.display;
}

void merge(LifecycleEffects& into, const LifecycleEffects& from) {
    into.checkpointRequested = into.checkpointRequested || from.checkpointRequested;
    into.gpuSubmissionStopped = into.gpuSubmissionStopped || from.gpuSubmissionStopped;
    into.renderRebuildRequested =
        into.renderRebuildRequested || from.renderRebuildRequested;
    if (from.display) {
        into.display = from.display;
    }
    if (from.memoryPressure) {
        into.memoryPressure =
            into.memoryPressure ? std::max(*into.memoryPressure, *from.memoryPressure)
                                : *from.memoryPressure;
    }
    if (from.thermal) {
        into.thermal = from.thermal;
    }
}

}  // namespace

// @implements spec/interface/mobile-platform.md Lifecycle
LifecycleEffects LifecycleState::apply(const LifecycleEvent& event) {
    LifecycleEffects effects;
    switch (event.kind) {
        case LifecycleEventKind::Pause:
            paused_ = true;
            break;
        case LifecycleEventKind::Resume:
            paused_ = false;
            break;
        case LifecycleEventKind::EnterBackground:
            // Only the transition asks for a checkpoint; a repeated callback
            // must not write the same state twice.
            if (!suspended_) {
                effects.checkpointRequested = true;
                effects.gpuSubmissionStopped = true;
            }
            suspended_ = true;
            break;
        case LifecycleEventKind::EnterForeground:
            suspended_ = false;
            break;
        case LifecycleEventKind::SurfaceAvailable:
            effects.display = requireMetrics(event);
            effects.renderRebuildRequested = true;
            surfaceAvailable_ = true;
            break;
        case LifecycleEventKind::SurfaceLost:
            if (surfaceAvailable_) {
                effects.gpuSubmissionStopped = true;
            }
            surfaceAvailable_ = false;
            break;
        case LifecycleEventKind::DisplayChanged:
            effects.display = requireMetrics(event);
            // Without a surface there is nothing to rebuild yet; the next
            // SurfaceAvailable carries fresh metrics anyway.
            effects.renderRebuildRequested = surfaceAvailable_;
            break;
        case LifecycleEventKind::MemoryPressure:
            effects.memoryPressure = event.memory;
            break;
        case LifecycleEventKind::ThermalPressure:
            effects.thermal = event.thermal;
            break;
    }
    return effects;
}

LifecycleEffects LifecycleState::apply(
    const std::span<const LifecycleEvent> events) {
    LifecycleEffects effects;
    for (const LifecycleEvent& event : events) {
        merge(effects, apply(event));
    }
    return effects;
}

bool LifecycleState::paused() const noexcept {
    return paused_;
}

bool LifecycleState::suspended() const noexcept {
    return suspended_;
}

bool LifecycleState::surfaceAvailable() const noexcept {
    return surfaceAvailable_;
}

bool LifecycleState::advancesSimulation() const noexcept {
    return !paused_ && !suspended_ && surfaceAvailable_;
}

bool LifecycleState::acceptsCommands() const noexcept {
    return advancesSimulation();
}

bool LifecycleState::maySubmitGpu() const noexcept {
    return !suspended_ && surfaceAvailable_;
}

}  // namespace konbini::app
