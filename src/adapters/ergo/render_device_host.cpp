#include "konbini/adapters/ergo/render_device_host.h"

#include <cmath>
#include <stdexcept>

#include <GLFW/glfw3.h>

#include "ergo/render/render_context.h"
#include "pictor/surface/glfw_surface_provider.h"
#include "pictor/surface/vulkan_context.h"

// @implements spec/interface/ergo-runtime.md Render host

namespace konbini::adapters::ergo {
namespace {

// Pictor の AttachmentRegistry 固定上限に合わせる。scene target は flight
// ごとに color / depth を持つため、ここを超えると offscreen pass が組めない。
constexpr std::uint32_t kMaxFramesInFlight = 4;

}  // namespace

struct RenderDeviceHost::Impl {
    ::pictor::GlfwSurfaceProvider surface;
    ::pictor::VulkanContext vulkan;
    ::ergo::render::RenderContext context;
    bool surfaceCreated = false;
    bool vulkanCreated = false;
};

RenderDeviceHost::RenderDeviceHost() : impl_(std::make_unique<Impl>()) {}

RenderDeviceHost::~RenderDeviceHost() {
    shutdown();
}

// @implements spec/interface/pictor-rendering.md Failure
void RenderDeviceHost::initialize(const RenderDeviceConfig& config) {
    if (isInitialized()) {
        throw std::logic_error("render device host is already initialized");
    }
    if (config.windowWidth == 0 || config.windowHeight == 0) {
        throw std::invalid_argument("window extent must be non-zero");
    }
    if (config.framesInFlight == 0 ||
        config.framesInFlight > kMaxFramesInFlight) {
        throw std::invalid_argument(
            "frames in flight must be between 1 and 4");
    }
    if (config.shaderDirectory.empty()) {
        throw std::invalid_argument(
            "render device host requires an explicit shader directory");
    }

    ::pictor::GlfwWindowConfig windowConfig;
    windowConfig.width = config.windowWidth;
    windowConfig.height = config.windowHeight;
    windowConfig.title = config.windowTitle;
    windowConfig.resizable = true;
    windowConfig.vsync = true;
    if (!impl_->surface.create(windowConfig)) {
        throw std::runtime_error("failed to create the GLFW window");
    }
    impl_->surfaceCreated = true;

    ::pictor::VulkanContextConfig vulkanConfig;
    vulkanConfig.app_name = "KonbiniDominant";
    vulkanConfig.validation = config.validation;
    // 既定 swapchain render pass は composite / HUD pass が使うので必須。
    vulkanConfig.create_default_render_pass = true;
    vulkanConfig.frames_in_flight = config.framesInFlight;
    if (!impl_->vulkan.initialize(&impl_->surface, vulkanConfig)) {
        // 確保済み resource は逆順に解放してから失敗させる。
        shutdown();
        throw std::runtime_error(
            "failed to initialize the Pictor Vulkan context");
    }
    impl_->vulkanCreated = true;

    if (impl_->vulkan.default_render_pass() == VK_NULL_HANDLE) {
        shutdown();
        throw std::runtime_error(
            "Pictor did not create the default swapchain render pass");
    }

    impl_->context.vk = &impl_->vulkan;
    impl_->context.surface = &impl_->surface;
    impl_->context.renderer = nullptr;
    impl_->context.anim = nullptr;
    impl_->context.shader_dir = config.shaderDirectory;
    impl_->context.asset_root = config.assetRoot;
}

void RenderDeviceHost::shutdown() noexcept {
    if (impl_ == nullptr) {
        return;
    }
    impl_->context.vk = nullptr;
    impl_->context.surface = nullptr;
    if (impl_->vulkanCreated) {
        impl_->vulkan.shutdown();
        impl_->vulkanCreated = false;
    }
    if (impl_->surfaceCreated) {
        impl_->surface.destroy();
        impl_->surfaceCreated = false;
    }
}

bool RenderDeviceHost::isInitialized() const noexcept {
    return impl_ != nullptr && impl_->vulkanCreated &&
           impl_->vulkan.is_initialized();
}

::pictor::VulkanContext& RenderDeviceHost::vulkan() {
    if (!isInitialized()) {
        throw std::logic_error("render device host is not initialized");
    }
    return impl_->vulkan;
}

::pictor::GlfwSurfaceProvider& RenderDeviceHost::surface() {
    if (!impl_->surfaceCreated) {
        throw std::logic_error("render device host has no window");
    }
    return impl_->surface;
}

::ergo::render::RenderContext& RenderDeviceHost::context() {
    if (!isInitialized()) {
        throw std::logic_error("render device host is not initialized");
    }
    return impl_->context;
}

GLFWwindow* RenderDeviceHost::window() const {
    if (!impl_->surfaceCreated) {
        throw std::logic_error("render device host has no window");
    }
    return impl_->surface.glfw_window();
}

void RenderDeviceHost::pollEvents() {
    if (!impl_->surfaceCreated) {
        throw std::logic_error("render device host has no window");
    }
    impl_->surface.poll_events();
}

void RenderDeviceHost::waitEvents(const double timeoutSeconds) {
    if (!impl_->surfaceCreated) {
        throw std::logic_error("render device host has no window");
    }
    if (!std::isfinite(timeoutSeconds) || timeoutSeconds <= 0.0) {
        throw std::invalid_argument(
            "event wait timeout must be finite and positive");
    }
    glfwWaitEventsTimeout(timeoutSeconds);
}

bool RenderDeviceHost::shouldClose() const {
    if (!impl_->surfaceCreated) {
        return true;
    }
    return impl_->surface.should_close();
}

render::ViewportExtent RenderDeviceHost::swapchainExtent() const {
    if (!impl_->vulkanCreated || !impl_->vulkan.is_initialized()) {
        return {};
    }
    const VkExtent2D extent = impl_->vulkan.swapchain_extent();
    return {.width = extent.width, .height = extent.height};
}

render::ViewportExtent RenderDeviceHost::framebufferExtent() const {
    if (!impl_->surfaceCreated) {
        return {};
    }
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(impl_->surface.glfw_window(), &width, &height);
    if (width <= 0 || height <= 0) {
        return {};
    }
    return {
        .width = static_cast<std::uint32_t>(width),
        .height = static_cast<std::uint32_t>(height),
    };
}

}  // namespace konbini::adapters::ergo
