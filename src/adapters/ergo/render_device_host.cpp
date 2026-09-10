#include "konbini/adapters/ergo/render_device_host.h"
#include "render_device_platform.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include "ergo/render/render_context.h"
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
        shutdown();
        throw std::runtime_error("failed to initialize the Pictor Vulkan context");
    }
    impl_->vulkanCreated=true;
    if(impl_->vulkan.default_render_pass()==VK_NULL_HANDLE) {
        shutdown();
        throw std::runtime_error("Pictor did not create the default swapchain render pass");
    }
    impl_->context.vk=&impl_->vulkan;
    // Pinned Ergo keeps a desktop-only optional surface pointer. The game host
    // handles mobile lifecycle; FrameComposer and game layers use context.vk.
#ifdef KONBINI_DESKTOP_WINDOW
    impl_->context.surface=impl_->desktop.get();
#else
    impl_->context.surface=nullptr;
#endif
    impl_->context.renderer=nullptr;impl_->context.anim=nullptr;
    impl_->context.shader_dir=config.shaderDirectory;impl_->context.asset_root=config.assetRoot;
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
