#include "konbini/app/platform/lifecycle_event.h"

// @implements spec/interface/mobile-platform.md Lifecycle

namespace konbini::app {

LifecycleEvent LifecycleEvent::pause() noexcept {
    return {.kind = LifecycleEventKind::Pause};
}

LifecycleEvent LifecycleEvent::resume() noexcept {
    return {.kind = LifecycleEventKind::Resume};
}

LifecycleEvent LifecycleEvent::enterBackground() noexcept {
    return {.kind = LifecycleEventKind::EnterBackground};
}

LifecycleEvent LifecycleEvent::enterForeground() noexcept {
    return {.kind = LifecycleEventKind::EnterForeground};
}

LifecycleEvent LifecycleEvent::surfaceAvailable(
    const DisplayMetrics& metrics) noexcept {
    return {.kind = LifecycleEventKind::SurfaceAvailable, .display = metrics};
}

LifecycleEvent LifecycleEvent::surfaceLost() noexcept {
    return {.kind = LifecycleEventKind::SurfaceLost};
}

LifecycleEvent LifecycleEvent::displayChanged(
    const DisplayMetrics& metrics) noexcept {
    return {.kind = LifecycleEventKind::DisplayChanged, .display = metrics};
}

LifecycleEvent LifecycleEvent::memoryPressure(
    const MemoryPressureLevel level) noexcept {
    return {.kind = LifecycleEventKind::MemoryPressure, .memory = level};
}

LifecycleEvent LifecycleEvent::thermalPressure(
    const ThermalLevel level) noexcept {
    return {.kind = LifecycleEventKind::ThermalPressure, .thermal = level};
}

}  // namespace konbini::app
