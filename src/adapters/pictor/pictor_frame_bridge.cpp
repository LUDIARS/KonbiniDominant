#include "konbini/adapters/pictor/pictor_frame_bridge.h"

#include <stdexcept>
#include <utility>
#include <vector>

#include "konbini/adapters/pictor/gpu_asset_store.h"
#include "konbini/adapters/pictor/konbini_batch_gpu_source.h"
#include "konbini/adapters/pictor/pictor_batch_plan.h"
#include "konbini/adapters/pictor/world_frustum.h"
#include "konbini/adapters/pictor/world_instance_buffers.h"
#include "konbini/adapters/pictor/world_instanced_pipelines.h"
#include "konbini/adapters/pictor/world_shader_keys.h"
#include "pictor/batch/batch_builder.h"
#include "pictor/culling/culling_system.h"
#include "pictor/memory/memory_subsystem.h"
#include "pictor/pipeline/compiled_batch_recorder.h"
#include "pictor/scene/scene_registry.h"

// @implements spec/interface/pictor-rendering.md Required bridge

namespace konbini::adapters::pictor {

namespace {

// Sort pairs + radix scratch + sorted indices for the DYNAMIC pool. 4 MiB per
// flight covers well over the first playable's facility count; exhaustion
// surfaces as "batches without sorted indices", never as dropped objects.
constexpr std::size_t kFrameAllocatorBytes = 4U * 1024U * 1024U;

}  // namespace

struct PictorFrameBridge::Impl {
    GpuAssetStore* assets = nullptr;
    std::uint32_t flightCount = 0;

    // Declaration order is construction order; members are destroyed in
    // reverse, which keeps every borrower ahead of what it borrows.
    std::unique_ptr<::pictor::MemorySubsystem> memory;
    std::unique_ptr<::pictor::SceneRegistry> registry;
    std::unique_ptr<::pictor::CullingSystem> culling;
    std::unique_ptr<::pictor::BatchBuilder> batches;
    std::unique_ptr<PictorSceneSync> sync;
    std::unique_ptr<KonbiniBatchGpuSource> source;
    std::unique_ptr<::pictor::CompiledBatchRecorder> recorder;

    WorldInstanceBuffers instanceBuffers;
    WorldInstancedPipelines pipelines;

    PictorBatchPlan plan;
    ::pictor::Frustum frustum{};
    WorldInstancedPushConstants push{};
    bool hasCamera = false;

    bool prepared = false;
    std::uint32_t preparedFlight = 0;
    VkDescriptorSet instanceSet = VK_NULL_HANDLE;
    PictorFrameStats stats;
};

PictorFrameBridge::PictorFrameBridge() : impl_(std::make_unique<Impl>()) {}

PictorFrameBridge::~PictorFrameBridge() {
    shutdown();
}

// @implements spec/interface/pictor-rendering.md Required bridge
void PictorFrameBridge::initialize(
    GpuAssetStore& assets, const std::uint32_t flightCount) {
    if (isInitialized()) {
        throw std::logic_error("Pictor frame bridge is already initialized");
    }
    if (!assets.isInitialized() || flightCount == 0) {
        throw std::invalid_argument(
            "Pictor frame bridge requires an initialized asset store and "
            "flights");
    }
    if (assets.retireLatencyFrames() < flightCount) {
        // Eviction must not outrun the flights that may still read a mesh.
        throw std::invalid_argument(
            "GPU asset retire latency is shorter than the flight count");
    }

    Impl& impl = *impl_;
    try {
        ::pictor::MemoryConfig memoryConfig;
        memoryConfig.frame_allocator_size = kFrameAllocatorBytes;
        memoryConfig.flight_count = flightCount;
        impl.memory = std::make_unique<::pictor::MemorySubsystem>(memoryConfig);
        impl.registry = std::make_unique<::pictor::SceneRegistry>(*impl.memory);
        impl.culling = std::make_unique<::pictor::CullingSystem>(*impl.registry);
        impl.batches = std::make_unique<::pictor::BatchBuilder>(*impl.registry);
        impl.sync = std::make_unique<PictorSceneSync>(*impl.registry, assets);
        impl.source = std::make_unique<KonbiniBatchGpuSource>(assets);
        // No MaterialRegistry: the batch's own shader key selects the pipeline.
        impl.recorder =
            std::make_unique<::pictor::CompiledBatchRecorder>(*impl.source);
    } catch (...) {
        impl.recorder.reset();
        impl.source.reset();
        impl.sync.reset();
        impl.batches.reset();
        impl.culling.reset();
        impl.registry.reset();
        impl.memory.reset();
        throw;
    }
    impl.assets = &assets;
    impl.flightCount = flightCount;
    impl.stats = {};
    impl.hasCamera = false;
    impl.prepared = false;
}

void PictorFrameBridge::shutdown() noexcept {
    if (impl_ == nullptr || impl_->assets == nullptr) {
        return;
    }
    Impl& impl = *impl_;
    detachDevice();
    // Release every mesh reference before the asset store goes away.
    impl.sync->clear(impl.stats.frameSerial);
    impl.recorder.reset();
    impl.source.reset();
    impl.sync.reset();
    impl.batches.reset();
    impl.culling.reset();
    impl.registry.reset();
    impl.memory.reset();
    impl.plan = {};
    impl.assets = nullptr;
    impl.flightCount = 0;
    impl.hasCamera = false;
    impl.prepared = false;
}

bool PictorFrameBridge::isInitialized() const noexcept {
    return impl_ != nullptr && impl_->assets != nullptr;
}

std::uint32_t PictorFrameBridge::flightCount() const noexcept {
    return impl_ == nullptr ? 0 : impl_->flightCount;
}

void PictorFrameBridge::attachDevice(
    const VkPhysicalDevice physicalDevice, const VkDevice device,
    const std::filesystem::path& shaderDirectory) {
    if (!isInitialized()) {
        throw std::logic_error(
            "Pictor frame bridge attach before initialization");
    }
    if (isAttached()) {
        throw std::logic_error("Pictor frame bridge is already attached");
    }
    Impl& impl = *impl_;
    try {
        impl.instanceBuffers.initialize(
            physicalDevice, device, impl.flightCount);
        impl.pipelines.initialize(
            device, shaderDirectory, impl.instanceBuffers.setLayout());
    } catch (...) {
        impl.pipelines.shutdown();
        impl.instanceBuffers.shutdown();
        throw;
    }
}

// @implements spec/interface/pictor-rendering.md World pass recording
void PictorFrameBridge::setRenderPass(const VkRenderPass renderPass) {
    if (!isAttached()) {
        throw std::logic_error(
            "Pictor frame bridge needs a device before a render pass");
    }
    Impl& impl = *impl_;
    impl.pipelines.setRenderPass(renderPass);
    const std::array<BatchPipelineBinding, 2> bindings{
        BatchPipelineBinding{
            .shaderKey = kOpaqueWorldShaderKey,
            .passType = ::pictor::PassType::OPAQUE,
            .pipeline = impl.pipelines.opaquePipeline(),
        },
        BatchPipelineBinding{
            .shaderKey = kTranslucentWorldShaderKey,
            .passType = ::pictor::PassType::TRANSPARENT,
            .pipeline = impl.pipelines.translucentPipeline(),
        },
    };
    impl.source->bindPipelines(bindings);
    impl.prepared = false;
}

void PictorFrameBridge::detachDevice() noexcept {
    if (impl_ == nullptr) {
        return;
    }
    Impl& impl = *impl_;
    if (impl.source != nullptr) {
        impl.source->clearPipelines();
    }
    impl.pipelines.shutdown();
    impl.instanceBuffers.shutdown();
    impl.instanceSet = VK_NULL_HANDLE;
    impl.prepared = false;
}

bool PictorFrameBridge::isAttached() const noexcept {
    return isInitialized() && impl_->instanceBuffers.isInitialized() &&
           impl_->pipelines.isInitialized();
}

VkRenderPass PictorFrameBridge::renderPass() const noexcept {
    return impl_ == nullptr ? VK_NULL_HANDLE : impl_->pipelines.renderPass();
}

// @implements spec/interface/pictor-rendering.md Object lifecycle
SceneSyncReport PictorFrameBridge::consume(
    const render::WorldDrawList& drawList,
    const std::array<float, 16>& viewProjection) {
    if (!isInitialized()) {
        throw std::logic_error(
            "Pictor frame bridge consume before initialization");
    }
    Impl& impl = *impl_;
    // Validate the camera before the scene changes.
    const ::pictor::Frustum frustum = frustumFromViewProjection(viewProjection);
    const std::vector<SceneObjectRequest> requests =
        sceneObjectRequests(drawList);
    const SceneSyncReport report =
        impl.sync->apply(requests, impl.stats.frameSerial);

    impl.frustum = frustum;
    impl.push.viewProjection = viewProjection;
    impl.hasCamera = true;
    impl.prepared = false;
    impl.stats.snapshotTick = drawList.snapshotTick;
    impl.stats.objects = static_cast<std::uint32_t>(impl.sync->objectCount());
    return report;
}

// @implements spec/interface/pictor-rendering.md Required bridge
void PictorFrameBridge::prepareFrame(const std::uint32_t flightIndex) {
    if (!isAttached() || !impl_->pipelines.hasPipelines()) {
        throw std::logic_error(
            "Pictor frame bridge prepare before device and render pass");
    }
    Impl& impl = *impl_;
    if (!impl.hasCamera) {
        throw std::logic_error(
            "Pictor frame bridge prepare before a snapshot was consumed");
    }
    if (flightIndex >= impl.flightCount) {
        throw std::out_of_range("Pictor frame bridge flight is out of range");
    }

    impl.memory->begin_frame();
    impl.culling->cull(impl.frustum, impl.memory->frame_allocator());
    impl.batches->build(impl.memory->frame_allocator());
    buildPictorBatchPlan(*impl.registry, *impl.batches, *impl.sync, impl.plan);
    impl.instanceSet = impl.instanceBuffers.upload(flightIndex, impl.plan.instances);

    ++impl.stats.frameSerial;
    impl.stats.visibleObjects = impl.culling->get_stats().visible_objects;
    impl.stats.batches = static_cast<std::uint32_t>(
        impl.plan.opaque.size() + impl.plan.translucent.size());
    impl.stats.drawCalls = 0;
    impl.stats.instances = 0;
    impl.stats.triangles = 0;
    impl.preparedFlight = flightIndex;
    impl.prepared = true;
}

void PictorFrameBridge::recordOpaque(
    const VkCommandBuffer commandBuffer, const VkExtent2D extent) {
    recordPass(commandBuffer, extent, false);
}

void PictorFrameBridge::recordTranslucent(
    const VkCommandBuffer commandBuffer, const VkExtent2D extent) {
    recordPass(commandBuffer, extent, true);
}

// @implements spec/interface/pictor-rendering.md Failure
void PictorFrameBridge::recordPass(
    const VkCommandBuffer commandBuffer, const VkExtent2D extent,
    const bool translucent) {
    if (!isAttached() || !impl_->prepared) {
        throw std::logic_error(
            "Pictor frame bridge record before prepareFrame");
    }
    if (commandBuffer == VK_NULL_HANDLE || extent.width == 0 ||
        extent.height == 0) {
        throw std::invalid_argument(
            "Pictor frame bridge record needs a command buffer and extent");
    }
    Impl& impl = *impl_;
    const std::vector<::pictor::RenderBatch>& batches =
        translucent ? impl.plan.translucent : impl.plan.opaque;
    if (batches.empty()) {
        return;
    }

    // Both instanced pipelines share this layout, so the set and push
    // constant stay bound across the recorder's vkCmdBindPipeline.
    const VkPipelineLayout layout = impl.pipelines.layout();
    vkCmdBindDescriptorSets(
        commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1,
        &impl.instanceSet, 0, nullptr);
    vkCmdPushConstants(
        commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
        static_cast<std::uint32_t>(sizeof(impl.push)), &impl.push);

    ::pictor::CompiledPass pass{};
    pass.render_area = {.offset = {0, 0}, .extent = extent};
    pass.pass_type = static_cast<std::uint8_t>(
        translucent ? ::pictor::PassType::TRANSPARENT
                    : ::pictor::PassType::OPAQUE);
    // The plan already split the lists; the recorder's own transparency
    // filter is set to the same category so both agree on every batch.
    pass.filter_mask = translucent ? ::pictor::RenderBatchFilter::TRANSPARENT
                                   : ::pictor::RenderBatchFilter::OPAQUE;
    pass.debug_name =
        translucent ? "konbini.world.translucent" : "konbini.world.opaque";

    impl.source->clearFailures();
    impl.recorder->begin_frame(&batches);
    impl.recorder->record(commandBuffer, pass, impl.preparedFlight, 0);

    const auto failures = impl.source->failures();
    if (!failures.empty()) {
        throw std::runtime_error(describeBatchResolveFailure(failures.front()));
    }
    const ::pictor::CompiledBatchRecorder::Stats& recorded =
        impl.recorder->stats();
    if (recorded.batches_skipped != 0) {
        throw std::runtime_error(
            "Pictor recorder skipped batches the GPU source resolved");
    }
    if (recorded.batches_filtered != 0) {
        throw std::logic_error(
            "Pictor recorder filtered batches the plan assigned to this pass");
    }
    impl.stats.drawCalls += recorded.draw_calls;
    impl.stats.instances += recorded.instances;
    impl.stats.triangles += recorded.triangles;
}

const PictorFrameStats& PictorFrameBridge::stats() const noexcept {
    return impl_->stats;
}

std::size_t PictorFrameBridge::objectCount() const noexcept {
    return isInitialized() ? impl_->sync->objectCount() : 0U;
}

std::optional<::pictor::ObjectId> PictorFrameBridge::objectFor(
    const sim::FacilityId facility) const noexcept {
    if (!isInitialized()) {
        return std::nullopt;
    }
    return impl_->sync->objectFor(facility);
}

}  // namespace konbini::adapters::pictor
