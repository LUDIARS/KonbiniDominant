#pragma once

#include <cstdint>
#include <optional>

#include "konbini/app/platform/display_metrics.h"
#include "konbini/app/platform/memory_pressure.h"

// @implements spec/interface/mobile-platform.md Lifecycle

namespace konbini::app {

// OS callbacks normalized by the platform host. The host only enqueues
// these; it never mutates the renderer or the simulation itself.
enum class LifecycleEventKind : std::uint8_t {
    Pause,             // inactive (call overlay, notification shade)
    Resume,
    EnterBackground,   // suspend
    EnterForeground,
    SurfaceAvailable,  // created or regained; carries display metrics
    SurfaceLost,
    DisplayChanged,    // resize / rotation / safe area; carries metrics
    MemoryPressure,
    ThermalPressure,
};

// @implements spec/interface/mobile-platform.md Lifecycle
struct LifecycleEvent {
    LifecycleEventKind kind = LifecycleEventKind::Pause;
    std::optional<DisplayMetrics> display;
    MemoryPressureLevel memory = MemoryPressureLevel::Normal;
    ThermalLevel thermal = ThermalLevel::Nominal;

    [[nodiscard]] static LifecycleEvent pause() noexcept;
    [[nodiscard]] static LifecycleEvent resume() noexcept;
    [[nodiscard]] static LifecycleEvent enterBackground() noexcept;
    [[nodiscard]] static LifecycleEvent enterForeground() noexcept;
    [[nodiscard]] static LifecycleEvent surfaceAvailable(
        const DisplayMetrics& metrics) noexcept;
    [[nodiscard]] static LifecycleEvent surfaceLost() noexcept;
    [[nodiscard]] static LifecycleEvent displayChanged(
        const DisplayMetrics& metrics) noexcept;
    [[nodiscard]] static LifecycleEvent memoryPressure(
        MemoryPressureLevel level) noexcept;
    [[nodiscard]] static LifecycleEvent thermalPressure(
        ThermalLevel level) noexcept;
};

}  // namespace konbini::app
