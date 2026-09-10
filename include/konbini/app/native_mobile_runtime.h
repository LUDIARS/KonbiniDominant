#pragma once
#include <filesystem>
#include <memory>
#include "konbini/app/touch_contacts.h"
#include "konbini/render/viewport_extent.h"
namespace pictor {class ISurfaceProvider;}
namespace konbini::app {
// All calls belong to one platform render thread. The game survives surface loss.
class NativeMobileRuntime {
public:
    explicit NativeMobileRuntime(const std::filesystem::path& assets);
    ~NativeMobileRuntime();
    void attach(::pictor::ISurfaceProvider& surface,render::ViewportExtent extent,double density);
    void detach() noexcept;
    void resize(render::ViewportExtent extent,double density);
    void pause(bool paused) noexcept;
    void touch(std::uint64_t id,TouchPhase phase,double xPixels,double yPixels);
    void frame(double monotonicSeconds);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
