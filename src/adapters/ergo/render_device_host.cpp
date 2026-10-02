#include "konbini/adapters/ergo/render_device_host.h"
#include "render_device_platform.h"
#include "konbini/adapters/ergo/render_readiness.h"
#include "konbini/adapters/ergo/render_init_error.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include "ergo/render/render_context.h"
#include "pictor/surface/frame_gate.h"
#include "pictor/surface/surface_provider.h"
#include "pictor/surface/vulkan_context.h"
#ifdef KONBINI_DESKTOP_WINDOW
#include <GLFW/glfw3.h>
#include "pictor/surface/glfw_surface_provider.h"
#endif
namespace konbini::adapters::ergo {
struct RenderDeviceHost::Impl {
#ifdef KONBINI_DESKTOP_WINDOW
    std::unique_ptr<::pictor::GlfwSurfaceProvider> desktop;
#endif
    ::pictor::ISurfaceProvider* surface=nullptr;
    ::pictor::VulkanContext vulkan;
    ::ergo::render::RenderContext context;
    bool vulkanCreated=false;
};
RenderDeviceHost::RenderDeviceHost():impl_(std::make_unique<Impl>()) {}
RenderDeviceHost::~RenderDeviceHost() {shutdown();}
void RenderDeviceHost::initialize(const RenderDeviceConfig& config) {
#ifdef KONBINI_DESKTOP_WINDOW
    if(impl_->surface) throw std::logic_error("render host is already attached");
    impl_->desktop=std::make_unique<::pictor::GlfwSurfaceProvider>();
    ::pictor::GlfwWindowConfig window;
    window.width=config.windowWidth;window.height=config.windowHeight;
    window.title=config.windowTitle;window.resizable=true;window.vsync=true;
    if(!impl_->desktop->create(window)) {
        impl_->desktop.reset();
        throw std::runtime_error("failed to create the GLFW window");
    }
    try {initialize(config,*impl_->desktop);}
    catch(...) {shutdown();throw;}
#else
    (void)config;
    throw std::logic_error("mobile render host requires a native surface");
#endif
}
void RenderDeviceHost::initialize(const RenderDeviceConfig& config,::pictor::ISurfaceProvider& surface) {
    if(impl_->surface) throw std::logic_error("render host is already attached");
    if(!config.windowWidth || !config.windowHeight || !config.framesInFlight || config.framesInFlight>4 ||
       config.shaderDirectory.empty()) throw std::invalid_argument("invalid render device configuration");
    impl_->surface=&surface;
    ::pictor::VulkanContextConfig vk;
    vk.app_name="KonbiniDominant";vk.validation=config.validation;
    vk.create_default_render_pass=true;vk.frames_in_flight=config.framesInFlight;
    if(!impl_->vulkan.initialize(&surface,vk)) {
        // Keep Pictor's typed reason: a missing native window is retryable
        // after the host hands the surface back, a capability gap is not.
        const ::pictor::ContextInitResult result=impl_->vulkan.init_result();
        shutdown();
        throw RenderInitError(result);
    }
    impl_->vulkanCreated=true;
    if(impl_->vulkan.default_render_pass()==VK_NULL_HANDLE) {
        shutdown();
        throw std::runtime_error("Pictor did not create the default swapchain render pass");
    }
    impl_->context.vk=&impl_->vulkan;
    // Pinned Ergo borrows the platform-neutral provider. Desktop passes the
    // GLFW provider and mobile hosts their native provider through the same
    // boundary; Ergo checks it every frame for surface loss.
    impl_->context.surface=&surface;
    impl_->context.renderer=nullptr;impl_->context.anim=nullptr;
    impl_->context.shader_dir=config.shaderDirectory;impl_->context.asset_root=config.assetRoot;
    // Never continue as a render-less host: a missing backend or surface is a
    // startup error, not a headless success.
    try {requireRenderReady(impl_->context,"render device host");}
    catch(...) {shutdown();throw;}
}
void RenderDeviceHost::shutdown() noexcept {
    if(!impl_) return;
    impl_->context.vk=nullptr;impl_->context.surface=nullptr;
    if(impl_->vulkanCreated) {impl_->vulkan.shutdown();impl_->vulkanCreated=false;}
    impl_->surface=nullptr;
#ifdef KONBINI_DESKTOP_WINDOW
    impl_->desktop.reset();
#endif
}
// @implements spec/interface/pictor-rendering.md Surface / device recovery
void RenderDeviceHost::setPresentationSuspended(const bool suspended) noexcept {
    if(impl_ && impl_->vulkanCreated) impl_->vulkan.set_presentation_suspended(suspended);
}
// @implements spec/interface/pictor-rendering.md Surface / device recovery
::pictor::FrameResult RenderDeviceHost::gateFrame() const noexcept {
    // Same predicate Pictor applies inside acquire / present and Ergo applies
    // before acquire. Evaluating it first keeps a stale last_frame_result()
    // from being read when Ergo returns early on a missing native window.
    ::pictor::FrameGateInput input;
    input.initialized=isInitialized();
    if(input.initialized) {
        input.latched=impl_->vulkan.last_frame_result().status;
        input.presentation_suspended=impl_->vulkan.presentation_suspended();
    }
    input.native_surface_available=impl_->surface!=nullptr &&
        impl_->surface->get_native_handle().type!=::pictor::NativeWindowHandle::Type::None;
    return ::pictor::gate_frame(input);
}
bool RenderDeviceHost::isInitialized() const noexcept {
    return impl_ && impl_->vulkanCreated && impl_->vulkan.is_initialized();
}
::pictor::VulkanContext& RenderDeviceHost::vulkan() {
    if(!isInitialized()) throw std::logic_error("render device host is not initialized");
    return impl_->vulkan;
}
::pictor::ISurfaceProvider& RenderDeviceHost::surface() {
    if(!impl_->surface) throw std::logic_error("render host has no surface");
    return *impl_->surface;
}
::ergo::render::RenderContext& RenderDeviceHost::context() {
    if(!isInitialized()) throw std::logic_error("render device host is not initialized");
    return impl_->context;
}
GLFWwindow* RenderDeviceHost::window() const {
#ifdef KONBINI_DESKTOP_WINDOW
    if(impl_->desktop) return impl_->desktop->glfw_window();
#endif
    throw std::logic_error("render host has no desktop window");
}
void RenderDeviceHost::pollEvents() {surface().poll_events();}
void RenderDeviceHost::waitEvents(const double seconds) {
    if(!std::isfinite(seconds) || seconds<=0) throw std::invalid_argument("invalid event wait duration");
#ifdef KONBINI_DESKTOP_WINDOW
    if(impl_->desktop) {glfwWaitEventsTimeout(seconds);return;}
#endif
    throw std::logic_error("mobile frames must use the platform display scheduler");
}
bool RenderDeviceHost::shouldClose() const {
    return !impl_->surface || impl_->surface->should_close();
}
render::ViewportExtent RenderDeviceHost::swapchainExtent() const {
    if(!isInitialized()) return {};
    const auto extent=impl_->vulkan.swapchain_extent();
    return {extent.width,extent.height};
}
render::ViewportExtent RenderDeviceHost::framebufferExtent() const {
    if(!impl_->surface) return {};
#ifdef KONBINI_DESKTOP_WINDOW
    if(impl_->desktop) {
        int width=0,height=0;
        glfwGetFramebufferSize(impl_->desktop->glfw_window(),&width,&height);
        return {static_cast<std::uint32_t>(std::max(0,width)),static_cast<std::uint32_t>(std::max(0,height))};
    }
#endif
    const auto config=impl_->surface->get_swapchain_config();
    return {config.width,config.height};
}
}
