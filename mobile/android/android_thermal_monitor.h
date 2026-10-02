#pragma once

#include <optional>

#include "konbini/app/platform/lifecycle_event_queue.h"
#include "konbini/app/platform/memory_pressure.h"

namespace konbini::android_host {

// Forwards `AThermal` status changes into the lifecycle queue. The thermal
// API exists from API 30 while the package supports API 29, so the symbols
// are resolved at runtime; without them the monitor reports no status and the
// profile selection records "thermal unavailable" instead of guessing.
// The listener runs on a binder thread and only pushes (noexcept).
// @implements spec/interface/mobile-platform.md Lifecycle
class AndroidThermalMonitor {
public:
    explicit AndroidThermalMonitor(app::LifecycleEventQueue& events);
    ~AndroidThermalMonitor();

    AndroidThermalMonitor(const AndroidThermalMonitor&) = delete;
    AndroidThermalMonitor& operator=(const AndroidThermalMonitor&) = delete;

    // Status at boot; empty when the device offers no thermal status.
    [[nodiscard]] std::optional<app::ThermalLevel> current() const noexcept;
    [[nodiscard]] bool available() const noexcept;

    // Runtime-resolved android/thermal.h entry points.
    struct Api;

private:
    static void onStatus(void* self, int status);

    app::LifecycleEventQueue& events_;
    const Api* api_ = nullptr;
    void* manager_ = nullptr;
    bool listening_ = false;
};

}  // namespace konbini::android_host
