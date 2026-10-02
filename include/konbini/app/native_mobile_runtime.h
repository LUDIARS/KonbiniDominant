#pragma once
#include <filesystem>
#include <memory>
#include "konbini/adapters/ergo/render_lifecycle.h"
#include "konbini/app/platform/display_metrics.h"
#include "konbini/app/touch_contacts.h"
#include "konbini/render/viewport_extent.h"
namespace pictor {class ISurfaceProvider;}
namespace konbini::app {
// All calls belong to one platform render thread. The game survives surface loss.
// @implements spec/interface/pictor-rendering.md Surface / device recovery
class NativeMobileRuntime {
public:
    explicit NativeMobileRuntime(const std::filesystem::path& assets);
    ~NativeMobileRuntime();
    // Initializes the render device on a host-owned surface. Throws
    // `adapters::ergo::RenderInitError` with Pictor's typed init status.
    void attach(::pictor::ISurfaceProvider& surface,render::ViewportExtent extent,double density);
    // Suspends presentation, then releases GPU resources (surface released).
    void detach() noexcept;
    // Explicit teardown + reinitialize on the attached surface after
    // `renderState()` reports ReinitializeRequired (surface / device lost).
    // Throws when reinitialize keeps failing without a presented frame.
    void reinitializeRender();
    void resize(render::ViewportExtent extent,double density);
    // Display change from the host (rotation, resize, safe area, density).
    // Validates, then resizes; the HUD follows the safe area on the next
    // frame. Selection and simulation state are kept.
    void displayMetrics(const DisplayMetrics& metrics);
    void pause(bool paused) noexcept;
    // Normalized native contact. Dropped while presentation may not submit
    // (paused, background, surface lost) so no contact survives a pause.
    void touch(const TouchSample& sample);
    void touch(std::uint64_t id,TouchPhase phase,double xPixels,double yPixels);
    // Never retries a lost surface / device itself: it stops GPU work and
    // reports ReinitializeRequired through `renderState()`.
    void frame(double monotonicSeconds);
    [[nodiscard]] adapters::ergo::RenderLifecycleState renderState() const noexcept;
    [[nodiscard]] adapters::ergo::FrameOutcome renderLossCause() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
