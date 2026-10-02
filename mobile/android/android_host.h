#pragma once

#include <cstdint>
#include <memory>
#include <optional>

#include "android_thermal_monitor.h"
#include "konbini/app/platform/display_metrics.h"
#include "konbini/app/platform/graphics_profile_diagnostic.h"
#include "konbini/app/platform/lifecycle_event_queue.h"
#include "konbini/app/platform/lifecycle_state.h"
#include "konbini/app/platform/writable_paths.h"

struct android_app;
struct AInputEvent;

namespace pictor {
class AndroidSurfaceProvider;
}

namespace konbini::app {
class NativeMobileRuntime;
}

namespace konbini::android_host {

// Thin NativeActivity host. It owns only OS-facing objects (window provider,
// thermal listener) and normalizes callbacks into the shared lifecycle queue;
// the owner thread drains the queue and the shared `LifecycleState` decides
// what the game runtime does. It never touches cash, facilities, stores or
// phases.
// @implements spec/interface/mobile-platform.md Required boundaries
class AndroidHost {
public:
    explicit AndroidHost(android_app* app);
    ~AndroidHost();

    AndroidHost(const AndroidHost&) = delete;
    AndroidHost& operator=(const AndroidHost&) = delete;

    // Writable roots, packaged asset check / mirror, explicit graphics
    // profile + diagnostic, then the in-process Figmentum city plan.
    void boot();

    void onCommand(std::int32_t command);
    std::int32_t onInput(const AInputEvent* event);

    // Applies queued lifecycle events, then runs one frame when allowed.
    void step();

    [[nodiscard]] bool wantsFrames() const noexcept;
    [[nodiscard]] bool failed() const noexcept;

private:
    void attachWindow();
    void detachWindow() noexcept;
    void forwardActivityPause();
    void drainLifecycle();
    void runFrame();
    void push(const app::LifecycleEvent& event);
    [[nodiscard]] std::optional<app::DisplayMetrics> displayMetrics() const;
    [[nodiscard]] double density() const;
    void fail(const char* message) noexcept;

    android_app* app_ = nullptr;
    app::LifecycleEventQueue events_;
    app::LifecycleState state_;
    AndroidThermalMonitor thermal_;
    std::optional<app::WritablePathProvider> paths_;
    std::optional<app::GraphicsProfileDiagnostic> diagnostic_;
    std::unique_ptr<app::NativeMobileRuntime> game_;
    std::unique_ptr<pictor::AndroidSurfaceProvider> surface_;
    bool focused_ = false;
    bool resumed_ = false;
    bool pauseForwarded_ = true;
    bool failed_ = false;
};

}  // namespace konbini::android_host
