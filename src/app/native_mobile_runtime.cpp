#include "konbini/app/native_mobile_runtime.h"
#include "konbini/app/game_session.h"
#include "konbini/app/app_paths.h"
#include "konbini/adapters/ergo/render_device_host.h"
#include "konbini/adapters/ergo/world_frame_graph.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>
namespace konbini::app {
struct NativeMobileRuntime::Impl {
    explicit Impl(const std::filesystem::path& root)
        : assets(root),game(root/"data/content/first-playable.json") {}
    std::filesystem::path assets;
    GameSession game;
    adapters::ergo::RenderDeviceHost device;
    adapters::ergo::WorldFrameGraph graph;
    TouchContacts contacts;
    render::ViewportExtent extent;
    double density=1,lastSeconds=0;
    bool paused=false,attached=false;
};
NativeMobileRuntime::NativeMobileRuntime(const std::filesystem::path& assets):impl_(std::make_unique<Impl>(assets)) {}
NativeMobileRuntime::~NativeMobileRuntime() {detach();}
void NativeMobileRuntime::attach(::pictor::ISurfaceProvider& surface,render::ViewportExtent extent,double density) {
    detach();
    validateAppPaths({impl_->assets/"data/content/first-playable.json",impl_->assets/"shaders"});
    adapters::ergo::RenderDeviceConfig config;
    config.windowWidth=extent.width;config.windowHeight=extent.height;
    config.shaderDirectory=(impl_->assets/"shaders").string();config.assetRoot=(impl_->assets/"data").string();
    try {
        impl_->device.initialize(config,surface);
        impl_->graph.initialize(impl_->device);
        impl_->game.uploadGeometry(impl_->graph);
        impl_->attached=true;
        resize(extent,density);
    } catch(...) {detach();throw;}
}
void NativeMobileRuntime::detach() noexcept {
    impl_->game.suspend();impl_->contacts.cancel();impl_->lastSeconds=0;
    impl_->graph.shutdown();impl_->device.shutdown();impl_->attached=false;
}
void NativeMobileRuntime::resize(render::ViewportExtent extent,double density) {
    if(!std::isfinite(density) || density<=0) throw std::invalid_argument("invalid mobile display density");
    impl_->extent=extent;impl_->density=density;
    impl_->game.suspend();impl_->contacts.cancel();impl_->lastSeconds=0;
    if(impl_->attached && extent.width && extent.height && extent!=impl_->graph.extent())
        impl_->graph.requestRebuild();
}
void NativeMobileRuntime::pause(bool paused) noexcept {
    impl_->paused=paused;impl_->lastSeconds=0;impl_->game.suspend();impl_->contacts.cancel();
}
void NativeMobileRuntime::touch(std::uint64_t id,TouchPhase phase,double x,double y) {
    if(!impl_->paused) impl_->contacts.update(id,phase,x,y);
}
void NativeMobileRuntime::frame(double now) {
    if(!std::isfinite(now)) throw std::invalid_argument("invalid mobile frame time");
    const double dt=impl_->lastSeconds>0?std::max(0.0,now-impl_->lastSeconds):0;
    impl_->lastSeconds=now;
    if(!impl_->attached || impl_->paused || !impl_->extent.width || !impl_->extent.height) return;
    if(impl_->graph.rebuildPending()) {
        const auto outcome=impl_->graph.runFrame(0);
        if(adapters::ergo::isFatal(outcome)) throw std::runtime_error("mobile surface rebuild failed");
        impl_->contacts.cancel();return;
    }
    FrameInput input;
    input.dtSeconds=dt;input.viewport=impl_->graph.extent();input.uiScale=impl_->density;
    input.pointer=impl_->contacts.consume();
    input.cursorXPixels=input.pointer.xPixels;input.cursorYPixels=input.pointer.yPixels;
    input.cursorInsideViewport=input.cursorXPixels>=0 && input.cursorYPixels>=0 &&
        input.cursorXPixels<input.viewport.width && input.cursorYPixels<input.viewport.height;
    impl_->game.frame(input,input.viewport,impl_->graph);
    if(adapters::ergo::isFatal(impl_->graph.runFrame(static_cast<float>(dt))))
        throw std::runtime_error("mobile Pictor frame submission failed");
}
}
