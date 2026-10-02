#include "konbini/app/native_mobile_runtime.h"
#include "konbini/app/game_session.h"
#include "konbini/app/app_paths.h"
#include "konbini/adapters/ergo/render_device_host.h"
#include "konbini/adapters/ergo/world_frame_graph.h"
#include "konbini/adapters/figmentum/figmentum_city_adapter.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <string>
namespace konbini::app {
namespace {
// A reinitialize that never reaches a presented frame is a persistent device
// fault, not a recoverable loss. Stop instead of looping teardown / init.
constexpr unsigned kMaxReinitializeWithoutPresent=2;
}
struct NativeMobileRuntime::Impl {
    Impl(const std::filesystem::path& root,city::FacilityMeshDetail facilityMesh)
        : assets(root),cityGenerator(facilityMesh),game(root/"data/content/first-playable.json",cityGenerator) {}
    std::filesystem::path assets;
    // Kept for the session so its derived geometry cache can be evicted.
    adapters::figmentum::FigmentumCityAdapter cityGenerator;
    GameSession game;
    adapters::ergo::RenderDeviceHost device;
    adapters::ergo::WorldFrameGraph graph;
    adapters::ergo::RenderLifecycle lifecycle;
    TouchContacts contacts;
    ::pictor::ISurfaceProvider* surface=nullptr;
    render::ViewportExtent extent;
    SafeAreaInsets safeArea;
    double density=1,lastSeconds=0;
    unsigned reinitializeWithoutPresent=0;
    void applyPresentationGate() noexcept {device.setPresentationSuspended(lifecycle.presentationSuspended());}
    // Simulation state is kept: only accumulated wall time and touches go.
    void holdSimulation() noexcept {game.suspend();contacts.cancel();lastSeconds=0;}
};
NativeMobileRuntime::NativeMobileRuntime(const std::filesystem::path& assets)
    :NativeMobileRuntime(assets,city::kFirstPlayableFacilityMeshDetail) {}
NativeMobileRuntime::NativeMobileRuntime(const std::filesystem::path& assets,const city::FacilityMeshDetail facilityMesh)
    :impl_(std::make_unique<Impl>(assets,facilityMesh)) {}
NativeMobileRuntime::~NativeMobileRuntime() {detach();}
// @implements spec/interface/pictor-rendering.md Surface / device recovery
void NativeMobileRuntime::attach(::pictor::ISurfaceProvider& surface,render::ViewportExtent extent,double density) {
    detach();
    validateAppPaths({impl_->assets/"data/content/first-playable.json",impl_->assets/"shaders"});
    adapters::ergo::RenderDeviceConfig config;
    config.windowWidth=extent.width;config.windowHeight=extent.height;
    config.shaderDirectory=(impl_->assets/"shaders").string();config.assetRoot=(impl_->assets/"data").string();
    try {
        // Pictor recovery order 5.2 steps 6-7: context, then host-owned
        // swapchain dependents, then the game's geometry on the new device.
        impl_->device.initialize(config,surface);
        impl_->graph.initialize(impl_->device);
        impl_->game.uploadGeometry(impl_->graph);
        impl_->surface=&surface;
        impl_->lifecycle.attached();
        resize(extent,density);
    } catch(...) {detach();throw;}
}
// @implements spec/interface/pictor-rendering.md Surface / device recovery
void NativeMobileRuntime::detach() noexcept {
    impl_->holdSimulation();
    // Pictor recovery order 5.2 steps 1-4: stop submission first, then the
    // graph waits for the device and releases host-owned GPU resources
    // before the context goes. The game session is not touched.
    impl_->device.setPresentationSuspended(true);
    impl_->graph.shutdown();impl_->device.shutdown();
    impl_->lifecycle.detached();impl_->surface=nullptr;
}
// @implements spec/interface/pictor-rendering.md Surface / device recovery
void NativeMobileRuntime::reinitializeRender() {
    if(!impl_->surface) throw std::logic_error("mobile render reinitialize requires an attached surface");
    if(++impl_->reinitializeWithoutPresent>kMaxReinitializeWithoutPresent)
        throw std::runtime_error(std::string("mobile render keeps failing after reinitialize: ")+
            adapters::ergo::describeFrameOutcome(impl_->lifecycle.lossCause()));
    // Pictor recovery order 5.3: the native window stays with the host, the
    // device and every dependent resource are rebuilt. Never a resize.
    auto& surface=*impl_->surface;
    attach(surface,impl_->extent,impl_->density);
}
void NativeMobileRuntime::resize(render::ViewportExtent extent,double density) {
    if(!std::isfinite(density) || density<=0) throw std::invalid_argument("invalid mobile display density");
    impl_->extent=extent;impl_->density=density;
    impl_->holdSimulation();
    impl_->lifecycle.setDrawableArea(extent.width && extent.height);
    impl_->applyPresentationGate();
    if(impl_->lifecycle.state()!=adapters::ergo::RenderLifecycleState::Detached &&
       extent.width && extent.height && extent!=impl_->graph.extent())
        impl_->graph.requestRebuild();
}
void NativeMobileRuntime::displayMetrics(const DisplayMetrics& metrics) {
    validateDisplayMetrics(metrics);
    impl_->safeArea=metrics.safeArea;
    resize(metrics.extentPixels,metrics.density);
}
void NativeMobileRuntime::pause(bool paused) noexcept {
    impl_->lifecycle.setPaused(paused);
    impl_->applyPresentationGate();
    impl_->holdSimulation();
}
// @implements spec/interface/mobile-platform.md Lifecycle
std::size_t NativeMobileRuntime::memoryPressure(const MemoryPressureLevel level) {
    return impl_->cityGenerator.evictDerivedGeometry(geometryEvictionScope(level));
}
void NativeMobileRuntime::touch(const TouchSample& sample) {
    if(impl_->lifecycle.maySubmit()) impl_->contacts.update(sample);
}
void NativeMobileRuntime::touch(std::uint64_t id,TouchPhase phase,double x,double y) {
    if(impl_->lifecycle.maySubmit()) impl_->contacts.update(id,phase,x,y);
}
adapters::ergo::RenderLifecycleState NativeMobileRuntime::renderState() const noexcept {
    return impl_->lifecycle.state();
}
adapters::ergo::FrameOutcome NativeMobileRuntime::renderLossCause() const noexcept {
    return impl_->lifecycle.lossCause();
}
// @implements spec/interface/pictor-rendering.md Surface / device recovery
void NativeMobileRuntime::frame(double now) {
    if(!std::isfinite(now)) throw std::invalid_argument("invalid mobile frame time");
    const double dt=impl_->lastSeconds>0?std::max(0.0,now-impl_->lastSeconds):0;
    impl_->lastSeconds=now;
    if(!impl_->lifecycle.maySubmit()) {impl_->lastSeconds=0;return;}
    adapters::ergo::FrameOutcome outcome;
    if(impl_->graph.rebuildPending()) {
        outcome=impl_->graph.runFrame(0);
        impl_->contacts.cancel();
    } else {
        FrameInput input;
        input.dtSeconds=dt;input.viewport=impl_->graph.extent();input.uiScale=impl_->density;input.safeArea=impl_->safeArea;
        input.pointer=impl_->contacts.consume();
        input.cursorXPixels=input.pointer.xPixels;input.cursorYPixels=input.pointer.yPixels;
        input.cursorInsideViewport=input.cursorXPixels>=0 && input.cursorYPixels>=0 &&
            input.cursorXPixels<input.viewport.width && input.cursorYPixels<input.viewport.height;
        impl_->game.frame(input,input.viewport,impl_->graph);
        outcome=impl_->graph.runFrame(static_cast<float>(dt));
        if(adapters::ergo::wasPresented(outcome)) {impl_->game.notifyPresented();impl_->reinitializeWithoutPresent=0;}
    }
    impl_->lifecycle.observe(outcome);
    if(adapters::ergo::requiresReinitialize(outcome)) {
        // Surface / device loss: keep the simulation, stop GPU submission and
        // leave teardown + reinitialize to the platform host. No retry here.
        impl_->holdSimulation();
        impl_->applyPresentationGate();
    }
}
}
