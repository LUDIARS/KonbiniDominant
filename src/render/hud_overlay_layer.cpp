#include "konbini/render/hud_overlay_layer.h"

#include <cstdint>
#include <exception>
#include <filesystem>
#include <stdexcept>
#include <utility>

#include "ergo/render/render_context.h"
#include "konbini/adapters/pictor/world_overlay_buffers.h"
#include "hud_pipelines.h"
#include "pictor/surface/vulkan_context.h"

// @implements spec/feature/ui-ux.md Common HUD

namespace konbini::render {

struct HudOverlayLayer::Impl {
    ::ergo::render::RenderContext* context = nullptr;
    VkDevice device = VK_NULL_HANDLE;
    HudPipelines pipelines;
    adapters::pictor::WorldOverlayBuffers overlayBuffers;
    WorldMesh mesh;
    ViewportExtent extent{};
    bool hasFrame = false;
    bool initialized = false;
};

HudOverlayLayer::HudOverlayLayer() : impl_(std::make_unique<Impl>()) {}

HudOverlayLayer::~HudOverlayLayer() {
    shutdown();
}

// @implements spec/feature/ui-ux.md Common HUD
void HudOverlayLayer::initialize(::ergo::render::RenderContext& context) {
    if (impl_->context != nullptr || impl_->initialized) {
        throw std::logic_error("hud overlay layer is already initialized");
    }
    if (context.vk == nullptr || !context.vk->is_initialized() ||
        context.vk->device() == VK_NULL_HANDLE ||
        context.vk->physical_device() == VK_NULL_HANDLE ||
        context.vk->default_render_pass() == VK_NULL_HANDLE ||
        context.shader_dir.empty()) {
        throw std::invalid_argument(
            "hud overlay layer initialization prerequisites failed");
    }

    const std::uint32_t flightCount = context.vk->frames_in_flight();
    if (flightCount == 0) {
        throw std::invalid_argument(
            "hud overlay layer requires at least one frame in flight");
    }

    impl_->context = &context;
    impl_->device = context.vk->device();
    try {
        impl_->pipelines.initialize(
            impl_->device, std::filesystem::path(context.shader_dir));
        impl_->overlayBuffers.initialize(
            context.vk->physical_device(), impl_->device, flightCount);
    } catch (...) {
        shutdown();
        throw;
    }
    impl_->initialized = true;
}

// @implements spec/interface/pictor-rendering.md Offscreen world composition
void HudOverlayLayer::set_render_pass(const VkRenderPass renderPass) {
    if (!impl_->initialized || impl_->context == nullptr ||
        impl_->context->vk == nullptr ||
        !impl_->context->vk->is_initialized() ||
        impl_->context->vk->device() != impl_->device ||
        renderPass == VK_NULL_HANDLE ||
        renderPass != impl_->context->vk->default_render_pass()) {
        // HUD を offscreen world pass へ記録すると composite 前の HDR target
        // へ焼き込まれ、画面には出ないまま「描いたつもり」になる。
        throw std::invalid_argument(
            "hud overlay layer only records into the Pictor default pass");
    }

    if (impl_->pipelines.hasPipeline()) {
        impl_->context->vk->device_wait_idle();
    }
    impl_->pipelines.setRenderPass(renderPass);
}

// @implements spec/feature/ui-ux.md Common HUD
void HudOverlayLayer::publishFrame(WorldMesh mesh, const ViewportExtent extent) {
    if (extent.width == 0 || extent.height == 0) {
        throw std::invalid_argument(
            "hud overlay layer requires a non-zero viewport");
    }
    impl_->mesh = std::move(mesh);
    impl_->extent = extent;
    impl_->hasFrame = true;
}

// @implements spec/feature/ui-ux.md Common HUD
void HudOverlayLayer::record(
    const VkCommandBuffer commandBuffer, const VkExtent2D extent) {
    if (!impl_->initialized || impl_->context == nullptr ||
        impl_->context->vk == nullptr ||
        !impl_->context->vk->is_initialized() ||
        impl_->context->vk->device() != impl_->device ||
        commandBuffer == VK_NULL_HANDLE ||
        !impl_->pipelines.hasPipeline()) {
        throw std::logic_error(
            "hud overlay layer record called before initialization");
    }
    if (!impl_->hasFrame) {
        throw std::logic_error(
            "hud overlay layer record called before a frame was published");
    }

    const std::uint32_t flightCount = impl_->context->vk->frames_in_flight();
    const VkExtent2D swapchainExtent = impl_->context->vk->swapchain_extent();
    if (extent.width == 0 || extent.height == 0 ||
        extent.width != swapchainExtent.width ||
        extent.height != swapchainExtent.height ||
        extent.width != impl_->extent.width ||
        extent.height != impl_->extent.height ||
        impl_->overlayBuffers.flightCount() != flightCount ||
        impl_->pipelines.renderPass() !=
            impl_->context->vk->default_render_pass()) {
        throw std::runtime_error("hud overlay layer has stale render state");
    }

    if (impl_->mesh.indices.empty()) {
        return;
    }

    const std::uint32_t flight = impl_->context->vk->current_frame();
    if (flight >= flightCount) {
        throw std::runtime_error(
            "Pictor current flight exceeds hud overlay buffers");
    }

    const VkViewport viewport{
        .x = 0.0F,
        .y = 0.0F,
        .width = static_cast<float>(extent.width),
        .height = static_cast<float>(extent.height),
        .minDepth = 0.0F,
        .maxDepth = 1.0F,
    };
    const VkRect2D scissor{.offset = {0, 0}, .extent = extent};
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdBindPipeline(
        commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        impl_->pipelines.pipeline());
    const HudPushConstants push{
        .viewportPixels = {
            static_cast<float>(extent.width),
            static_cast<float>(extent.height), 0.0F, 0.0F}};
    vkCmdPushConstants(
        commandBuffer, impl_->pipelines.layout(), VK_SHADER_STAGE_VERTEX_BIT,
        0, static_cast<std::uint32_t>(sizeof(push)), &push);

    // acquire_next_image() がこの flight の fence を待った後なので、同 flight
    // の buffer を上書きしても直前の submit とは競合しない。
    const adapters::pictor::WorldGeometryBuffer& buffer =
        impl_->overlayBuffers.upload(flight, impl_->mesh);
    if (buffer.indexCount() == 0) {
        return;
    }
    const VkBuffer vertexBuffer = buffer.vertexBuffer();
    const VkDeviceSize vertexOffset = 0;
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &vertexOffset);
    vkCmdBindIndexBuffer(
        commandBuffer, buffer.indexBuffer(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, buffer.indexCount(), 1, 0, 0, 0);
}

// @implements spec/feature/ui-ux.md Common HUD
void HudOverlayLayer::shutdown() {
    if (impl_ == nullptr || impl_->context == nullptr) {
        return;
    }

    if (impl_->context->vk == nullptr ||
        !impl_->context->vk->is_initialized() ||
        impl_->context->vk->device() != impl_->device) {
        // 生の Vulkan handle は放棄できない。要求される寿命は hud layer ->
        // VulkanContext。
        std::terminate();
    }

    impl_->context->vk->device_wait_idle();
    impl_->overlayBuffers.shutdown();
    impl_->pipelines.shutdown();
    impl_->mesh = {};
    impl_->extent = {};
    impl_->hasFrame = false;
    impl_->device = VK_NULL_HANDLE;
    impl_->context = nullptr;
    impl_->initialized = false;
}

}  // namespace konbini::render
