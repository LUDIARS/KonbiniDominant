#include "konbini/adapters/ergo/world_frame_graph.h"

#include <array>
#include <optional>
#include <stdexcept>
#include <vector>

#include "ergo/render/frame_composer.h"
#include "ergo/render/frame_context.h"
#include "ergo/render/render_context.h"
#include "konbini/adapters/ergo/layer_initialization_scope.h"
#include "konbini/adapters/ergo/render_device_host.h"
#include "konbini/adapters/ergo/tracked_render_layer.h"
#include "konbini/adapters/pictor/world_geometry_cache.h"
#include "konbini/adapters/pictor/world_scene_targets.h"
#include "konbini/render/hud_overlay_layer.h"
#include "konbini/render/world_composite_layer.h"
#include "konbini/render/world_render_layer.h"
#include "pictor/surface/vulkan_context.h"

// @implements spec/interface/pictor-rendering.md Offscreen world composition

namespace konbini::adapters::ergo {
namespace {

// pass 0 は swapchain image index ではなく、記録中の flight に対応する
// framebuffer を使う (pictor-rendering.md#Offscreen world composition)。
VkFramebuffer worldFramebufferProvider(
    std::uint32_t /*imageIndex*/, void* user) {
    auto* const targets =
        static_cast<pictor::WorldSceneTargets*>(user);
    if (targets == nullptr || !targets->isInitialized()) {
        return VK_NULL_HANDLE;
    }
    return targets->currentFramebuffer();
}

// pass 0 の color write と pass 1 の sampling の間には、Pictor の registry が
// 作る external -> subpass 依存しか無いので、pass 間 hook で memory
// dependency を足す。
void compositeBarrierHook(
    VkCommandBuffer commandBuffer, std::uint32_t /*imageIndex*/,
    const ::ergo::render::FrameContext& /*frame*/, void* user) {
    auto* const targets =
        static_cast<pictor::WorldSceneTargets*>(user);
    if (targets == nullptr || !targets->isInitialized()) {
        return;
    }
    targets->recordColorShaderReadBarrier(commandBuffer);
}

}  // namespace

struct WorldFrameGraph::Impl {
    RenderDeviceHost* host = nullptr;
    pictor::WorldSceneTargets targets;
    pictor::WorldGeometryCache geometryCache;
    std::optional<render::WorldRenderLayer> worldLayer;
    std::optional<render::WorldCompositeLayer> compositeLayer;
    render::HudOverlayLayer hudLayer;
    std::optional<TrackedRenderLayer> trackedWorld;
    std::optional<TrackedRenderLayer> trackedComposite;
    std::optional<TrackedRenderLayer> trackedHud;
    LayerInitializationScope scope;
    std::unique_ptr<::ergo::render::FrameComposer> composer;
    std::uint64_t rebuildCount = 0;
    bool rebuildPending = false;
    bool initialized = false;
};

WorldFrameGraph::WorldFrameGraph() : impl_(std::make_unique<Impl>()) {}

WorldFrameGraph::~WorldFrameGraph() {
    shutdown();
}

// @implements spec/interface/pictor-rendering.md Offscreen world composition
void WorldFrameGraph::initialize(RenderDeviceHost& host) {
    if (impl_->initialized) {
        throw std::logic_error("world frame graph is already initialized");
    }
    if (!host.isInitialized()) {
        throw std::invalid_argument(
            "world frame graph requires an initialized render device");
    }

    impl_->host = &host;
    impl_->rebuildCount = 0;
    impl_->rebuildPending = false;
    try {
        impl_->targets.initialize(host.vulkan());
        impl_->geometryCache.initialize(
            host.vulkan().physical_device(), host.vulkan().device());
        impl_->worldLayer.emplace(impl_->targets, impl_->geometryCache);
        impl_->compositeLayer.emplace(impl_->targets);
        buildComposer();
    } catch (...) {
        // 逆順解放。composer -> layer -> geometry cache -> scene target。
        resetComposer();
        impl_->compositeLayer.reset();
        impl_->worldLayer.reset();
        impl_->geometryCache.shutdown();
        impl_->targets.shutdown();
        impl_->host = nullptr;
        throw;
    }
    impl_->initialized = true;
}

// @implements spec/interface/ergo-runtime.md Render host
void WorldFrameGraph::buildComposer() {
    ::pictor::VulkanContext& vulkan = impl_->host->vulkan();
    impl_->trackedWorld.emplace(*impl_->worldLayer, impl_->scope);
    impl_->trackedComposite.emplace(*impl_->compositeLayer, impl_->scope);
    impl_->trackedHud.emplace(impl_->hudLayer, impl_->scope);

    auto composer = std::make_unique<::ergo::render::FrameComposer>();
    const std::array<VkClearValue, 2> worldClears =
        pictor::WorldSceneTargets::clearValues();
    composer->add_pass(
        impl_->targets.renderPass(),
        {&*impl_->trackedWorld},
        std::vector<VkClearValue>(worldClears.begin(), worldClears.end()));
    // pass 1 は composite が全画面を書くので clear は 1 枚 (color) だけ。
    composer->add_pass(
        vulkan.default_render_pass(),
        {&*impl_->trackedComposite, &*impl_->trackedHud});
    composer->set_framebuffer_provider(
        0, &worldFramebufferProvider, &impl_->targets);
    composer->set_pre_pass_hook(1, &compositeBarrierHook, &impl_->targets);

    try {
        // pinned Ergo は途中失敗した initialize の rollback を持たないので、
        // scope が初期化済み layer を逆順で解放する。
        composer->initialize(impl_->host->context());
    } catch (...) {
        impl_->scope.rollback();
        throw;
    }
    impl_->composer = std::move(composer);
    // Successful initialization transfers normal shutdown ownership to the
    // composer. The scope is only the exception-path rollback ledger.
    impl_->scope.release();
}

void WorldFrameGraph::shutdown() noexcept {
    if (impl_ == nullptr || !impl_->initialized) {
        return;
    }
    if (impl_->host != nullptr && impl_->host->isInitialized()) {
        impl_->host->vulkan().device_wait_idle();
    }
    // composer が layer を逆順に shutdown する (hud -> composite -> world)。
    resetComposer();
    impl_->compositeLayer.reset();
    impl_->worldLayer.reset();
    impl_->geometryCache.shutdown();
    impl_->targets.shutdown();
    impl_->host = nullptr;
    impl_->rebuildPending = false;
    impl_->initialized = false;
}

bool WorldFrameGraph::isInitialized() const noexcept {
    return impl_ != nullptr && impl_->initialized;
}

pictor::WorldGeometryCache& WorldFrameGraph::geometryCache() {
    if (!isInitialized()) {
        throw std::logic_error("world frame graph is not initialized");
    }
    return impl_->geometryCache;
}

render::WorldRenderLayer& WorldFrameGraph::worldLayer() {
    if (!isInitialized() || !impl_->worldLayer.has_value()) {
        throw std::logic_error("world frame graph is not initialized");
    }
    return *impl_->worldLayer;
}

render::HudOverlayLayer& WorldFrameGraph::hudLayer() {
    if (!isInitialized()) {
        throw std::logic_error("world frame graph is not initialized");
    }
    return impl_->hudLayer;
}

render::ViewportExtent WorldFrameGraph::extent() const {
    if (impl_ == nullptr || !impl_->initialized ||
        !impl_->targets.isInitialized()) {
        return {};
    }
    const VkExtent2D extent = impl_->targets.extent();
    return {.width = extent.width, .height = extent.height};
}

// @implements spec/interface/ergo-runtime.md Render host
FrameOutcome WorldFrameGraph::runFrame(const float deltaSeconds) {
    if (!isInitialized()) {
        throw std::logic_error("world frame graph is not initialized");
    }
    if (impl_->rebuildPending) {
        const render::ViewportExtent framebuffer =
            impl_->host->framebufferExtent();
        if (framebuffer.width == 0 || framebuffer.height == 0 ||
            !recreateSwapchainAndRebuild()) {
            return FrameOutcome::SkippedWhileMinimized;
        }
        // Rebuild clears the layers' published CPU frame state. Do not enter
        // the new composer until the app loop republishes at the new extent.
        return FrameOutcome::SkippedForRebuild;
    }
    if (impl_->composer == nullptr) {
        throw std::logic_error("world frame graph has no active composer");
    }
    ::pictor::VulkanContext& vulkan = impl_->host->vulkan();
    const VkExtent2D extent = vulkan.swapchain_extent();

    ::ergo::render::FrameContext frame;
    frame.dt = deltaSeconds;
    frame.extent = extent;
    frame.frame_index = impl_->composer->frame_count();

    const SwapchainIdentity before = sampleSwapchainIdentity(vulkan);
    const std::uint64_t framesBefore = impl_->composer->frame_count();
    if (!impl_->composer->run_frame(frame)) {
        // record / submit の失敗。回復手段が無いので host へ返す。
        throw std::runtime_error(
            "frame composer failed to record or submit a frame");
    }
    const SwapchainIdentity after = sampleSwapchainIdentity(vulkan);
    const bool presented = impl_->composer->frame_count() != framesBefore;
    // present していない frame だけ device lost を確認する。present 済みの
    // frame で待つと、正常時に毎 frame の同期待ちを増やしてしまう。
    const bool deviceLost =
        !presented && probeDeviceLost(vulkan.device());

    const FrameOutcome outcome =
        classifyFrameOutcome(before, after, presented, deviceLost);
    if (requiresDependentRebuild(outcome)) {
        rebuild();
    } else if (outcome == FrameOutcome::SkippedWhileMinimized) {
        // Pictor can encounter a minimize race after the app sampled a
        // non-zero framebuffer. Its internal recreate then has no usable
        // swapchain; remember to perform an explicit recreate after restore.
        impl_->rebuildPending = true;
    }
    return outcome;
}

bool WorldFrameGraph::rebuildPending() const noexcept {
    return impl_ != nullptr && impl_->rebuildPending;
}

void WorldFrameGraph::requestRebuild() noexcept {
    if (impl_ != nullptr && impl_->initialized) {
        impl_->rebuildPending = true;
    }
}

std::uint64_t WorldFrameGraph::rebuildCount() const noexcept {
    return impl_ == nullptr ? 0 : impl_->rebuildCount;
}

void WorldFrameGraph::resetComposer() {
    // scene view を参照する composite descriptor / pipeline を先に捨てる。
    // composer の shutdown が hud -> composite -> world の逆順で回す。
    impl_->composer.reset();
    impl_->scope.rollback();
    impl_->trackedHud.reset();
    impl_->trackedComposite.reset();
    impl_->trackedWorld.reset();
}

void WorldFrameGraph::finishRebuild() {
    const VkExtent2D extent = impl_->host->vulkan().swapchain_extent();

    if (extent.width == 0 || extent.height == 0) {
        // 最小化中は scene target を作れない。geometry cache と layer 実体は
        // 残したまま、次 frame へ再構築を持ち越す。
        impl_->rebuildPending = true;
        return;
    }

    impl_->targets.resize(extent);
    // layer 実体は生きているが GPU resource は解放済みなので、同じ順で
    // 初期化し直す。generation guard がずれていれば record 側で落ちる。
    buildComposer();
    impl_->rebuildPending = false;
    ++impl_->rebuildCount;
}

// @implements spec/interface/pictor-rendering.md Offscreen world composition
void WorldFrameGraph::rebuild() {
    impl_->host->vulkan().device_wait_idle();
    resetComposer();
    finishRebuild();
}

bool WorldFrameGraph::recreateSwapchainAndRebuild() {
    ::pictor::VulkanContext& vulkan = impl_->host->vulkan();
    vulkan.device_wait_idle();
    resetComposer();
    if (!vulkan.recreate_swapchain()) {
        impl_->rebuildPending = true;
        const render::ViewportExtent framebuffer =
            impl_->host->framebufferExtent();
        if (framebuffer.width == 0 || framebuffer.height == 0) {
            return false;
        }
        throw std::runtime_error("explicit swapchain recreation failed");
    }
    finishRebuild();
    return !impl_->rebuildPending;
}

}  // namespace konbini::adapters::ergo
