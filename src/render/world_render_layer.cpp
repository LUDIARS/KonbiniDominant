#include "konbini/render/world_render_layer.h"

#include <cstdint>
#include <exception>
#include <filesystem>
#include <stdexcept>
#include <utility>

#include "ergo/render/render_context.h"
#include "konbini/adapters/pictor/world_geometry_cache.h"
#include "konbini/adapters/pictor/world_overlay_buffers.h"
#include "konbini/adapters/pictor/world_scene_targets.h"
#include "pictor/surface/vulkan_context.h"
#include "world_pipelines.h"

// @implements spec/interface/pictor-rendering.md Offscreen world composition

namespace konbini::render {

namespace {

constexpr WorldVertex::ColorRgba kNeutralTint{1.0F, 1.0F, 1.0F, 1.0F};

}  // namespace

struct WorldRenderLayer::Impl {
    ::ergo::render::RenderContext* context = nullptr;
    VkDevice device = VK_NULL_HANDLE;
    WorldPipelines pipelines;
    adapters::pictor::WorldOverlayBuffers overlayBuffers;
    // 記録前に scene target の bundle 世代を検証する。extent と flight 数が
    // 変わらない resize では他の guard がすべて通ってしまい、破棄済み
    // framebuffer / render pass への記録をこの比較だけが捕捉できる。
    std::uint64_t targetsGeneration = 0;
    IsometricCamera camera;
    WorldDrawList drawList;
    bool hasFrame = false;
    bool initialized = false;
};

WorldRenderLayer::WorldRenderLayer(
    adapters::pictor::WorldSceneTargets& targets,
    adapters::pictor::WorldGeometryCache& geometryCache)
    : targets_(&targets),
      geometryCache_(&geometryCache),
      impl_(std::make_unique<Impl>()) {}

WorldRenderLayer::~WorldRenderLayer() {
    shutdown();
}

// @implements spec/interface/pictor-rendering.md Offscreen world composition
void WorldRenderLayer::initialize(::ergo::render::RenderContext& context) {
    if (impl_->context != nullptr || impl_->initialized) {
        throw std::logic_error("world render layer is already initialized");
    }
    if (context.vk == nullptr || !context.vk->is_initialized() ||
        context.vk->device() == VK_NULL_HANDLE ||
        context.vk->physical_device() == VK_NULL_HANDLE ||
        context.shader_dir.empty() || targets_ == nullptr ||
        !targets_->isInitialized() || geometryCache_ == nullptr ||
        !geometryCache_->isInitialized()) {
        throw std::invalid_argument(
            "world render layer initialization prerequisites failed");
    }

    const std::uint32_t flightCount = context.vk->frames_in_flight();
    if (flightCount == 0 || targets_->flightCount() != flightCount) {
        throw std::invalid_argument(
            "world scene targets do not match the Pictor frame host");
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
    impl_->targetsGeneration = targets_->generation();
    impl_->initialized = true;
}

// @implements spec/interface/pictor-rendering.md Offscreen world composition
void WorldRenderLayer::set_render_pass(const VkRenderPass renderPass) {
    if (!impl_->initialized || impl_->context == nullptr ||
        impl_->context->vk == nullptr ||
        !impl_->context->vk->is_initialized() ||
        impl_->context->vk->device() != impl_->device ||
        targets_ == nullptr || !targets_->isInitialized() ||
        targets_->generation() != impl_->targetsGeneration ||
        targets_->flightCount() !=
            impl_->context->vk->frames_in_flight() ||
        renderPass == VK_NULL_HANDLE) {
        throw std::invalid_argument(
            "world render layer requires live world scene targets");
    }
    // depth 付き world 描画を Pictor 既定 swapchain pass へ直接記録しない、
    // という pass 構成の不変条件。既定 pass には depth attachment が無く、
    // 記録できてしまうと depth 無しで都市が描かれる。
    if (renderPass != targets_->renderPass() ||
        renderPass == impl_->context->vk->default_render_pass()) {
        throw std::invalid_argument(
            "world render layer only records into the offscreen world pass");
    }

    if (impl_->pipelines.hasPipelines()) {
        // 旧 pipeline が in-flight でないことを保証してから作り直す。
        impl_->context->vk->device_wait_idle();
    }
    impl_->pipelines.setRenderPass(renderPass);
}

// @implements spec/interface/pictor-rendering.md `RenderSnapshot`
void WorldRenderLayer::publishFrame(
    const IsometricCamera& camera, WorldDrawList drawList) {
    if (camera.extent.width == 0 || camera.extent.height == 0) {
        throw std::invalid_argument(
            "world render layer requires a non-zero camera viewport");
    }
    impl_->camera = camera;
    impl_->drawList = std::move(drawList);
    impl_->hasFrame = true;
}

// @implements spec/interface/pictor-rendering.md Offscreen world composition
void WorldRenderLayer::record(
    const VkCommandBuffer commandBuffer, const VkExtent2D extent) {
    if (!impl_->initialized || impl_->context == nullptr ||
        impl_->context->vk == nullptr ||
        !impl_->context->vk->is_initialized() ||
        impl_->context->vk->device() != impl_->device ||
        targets_ == nullptr || !targets_->isInitialized() ||
        geometryCache_ == nullptr || !geometryCache_->isInitialized() ||
        commandBuffer == VK_NULL_HANDLE ||
        !impl_->pipelines.hasPipelines()) {
        throw std::logic_error(
            "world render layer record called before initialization");
    }
    if (!impl_->hasFrame) {
        throw std::logic_error(
            "world render layer record called before a frame was published");
    }

    const std::uint32_t flightCount =
        impl_->context->vk->frames_in_flight();
    const VkExtent2D targetExtent = targets_->extent();
    if (extent.width == 0 || extent.height == 0 ||
        extent.width != targetExtent.width ||
        extent.height != targetExtent.height ||
        extent.width != impl_->camera.extent.width ||
        extent.height != impl_->camera.extent.height ||
        targets_->generation() != impl_->targetsGeneration ||
        targets_->flightCount() != flightCount ||
        impl_->overlayBuffers.flightCount() != flightCount ||
        impl_->pipelines.renderPass() != targets_->renderPass()) {
        throw std::runtime_error(
            "world render layer has stale render targets");
    }

    const std::uint32_t flight = impl_->context->vk->current_frame();
    if (flight >= flightCount) {
        throw std::runtime_error(
            "Pictor current flight exceeds world overlay buffers");
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

    WorldPushConstants push{
        .viewProjection = impl_->camera.viewProjection,
        .tint = kNeutralTint,
    };
    const VkPipelineLayout layout = impl_->pipelines.layout();
    const auto recordFacility =
        [&](const WorldFacilityDraw& draw) {
            // 未登録 key はここで例外になる。missing geometry を飛ばすと
            // 「生成漏れ」と「そこに建物が無い」が区別できなくなる。
            const adapters::pictor::WorldGeometryBuffer& geometry =
                geometryCache_->find(draw.figmentumKey);
            if (geometry.indexCount() == 0) {
                throw std::runtime_error(
                    "cached facility geometry has no indices");
            }
            push.tint = draw.tint;
            vkCmdPushConstants(
                commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                static_cast<std::uint32_t>(sizeof(push)), &push);
            const VkBuffer vertexBuffer = geometry.vertexBuffer();
            const VkDeviceSize vertexOffset = 0;
            vkCmdBindVertexBuffers(
                commandBuffer, 0, 1, &vertexBuffer, &vertexOffset);
            vkCmdBindIndexBuffer(
                commandBuffer, geometry.indexBuffer(), 0,
                VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(
                commandBuffer, geometry.indexCount(), 1, 0, 0, 0);
        };

    // 記録順は base facility -> 半透明 facility -> ZOC / store / selection。
    // base だけが depth を書き、以降は書かれた depth に対して test する。
    vkCmdBindPipeline(
        commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        impl_->pipelines.basePipeline());
    for (const WorldFacilityDraw& draw : impl_->drawList.baseFacilities) {
        recordFacility(draw);
    }

    vkCmdBindPipeline(
        commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        impl_->pipelines.overlayPipeline());
    for (const WorldFacilityDraw& draw : impl_->drawList.overlayFacilities) {
        recordFacility(draw);
    }

    // acquire_next_image() がこの flight の fence を待った後なので、同 flight
    // の buffer を上書きしても直前の submit とは競合しない。
    const adapters::pictor::WorldGeometryBuffer& overlay =
        impl_->overlayBuffers.upload(flight, impl_->drawList.overlayMesh);
    if (overlay.indexCount() != 0) {
        push.tint = kNeutralTint;
        vkCmdPushConstants(
            commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
            static_cast<std::uint32_t>(sizeof(push)), &push);
        const VkBuffer vertexBuffer = overlay.vertexBuffer();
        const VkDeviceSize vertexOffset = 0;
        vkCmdBindVertexBuffers(
            commandBuffer, 0, 1, &vertexBuffer, &vertexOffset);
        vkCmdBindIndexBuffer(
            commandBuffer, overlay.indexBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(
            commandBuffer, overlay.indexCount(), 1, 0, 0, 0);
    }
}

// @implements spec/interface/pictor-rendering.md Offscreen world composition
void WorldRenderLayer::shutdown() {
    if (impl_ == nullptr || impl_->context == nullptr) {
        return;
    }

    if (impl_->context->vk == nullptr ||
        !impl_->context->vk->is_initialized() ||
        impl_->context->vk->device() != impl_->device) {
        // 生の Vulkan handle は放棄できない。要求される寿命は world layer ->
        // scene targets / geometry cache -> VulkanContext。
        std::terminate();
    }

    impl_->context->vk->device_wait_idle();
    impl_->overlayBuffers.shutdown();
    impl_->pipelines.shutdown();
    impl_->drawList = {};
    impl_->camera = {};
    impl_->hasFrame = false;
    impl_->targetsGeneration = 0;
    impl_->device = VK_NULL_HANDLE;
    impl_->context = nullptr;
    impl_->initialized = false;
}

}  // namespace konbini::render
