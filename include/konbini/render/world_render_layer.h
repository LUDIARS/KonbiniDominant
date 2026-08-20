#pragma once

#include <memory>

#include "konbini/render/isometric_camera.h"
#include "konbini/render/world_draw_list.h"

// Ergo's Vulkan forward header can include Win32 before Pictor parses
// ObjectFlags::TRANSPARENT and PassType::OPAQUE. Suppress the colliding GDI
// macros at this public boundary, matching Pictor's registry headers.
#ifndef NOGDI
#define NOGDI
#endif
#include "ergo/render/render_layer.h"

namespace konbini::adapters::pictor {
class WorldGeometryCache;
class WorldSceneTargets;
}  // namespace konbini::adapters::pictor

namespace konbini::render {

// offscreen world pass (pass 0) の記録担当。
//
// `WorldSceneTargets` が所有する custom render pass だけを対象にし、Pictor の
// 既定 swapchain render pass を渡されたら fail-fast する。depth 付きの都市
// 描画を既定 pass へ直接記録しないという pass 構成の不変条件を、layer 側でも
// 守るため。合成側は `WorldCompositeLayer` が pass 1 で行う。
//
// 破棄順は world layer -> scene targets / geometry cache -> VulkanContext。
// `targets`、`geometryCache`、`initialize()` に渡す RenderContext はすべて
// 借用で、この layer より長生きしなければならない。
//
// @implements spec/interface/pictor-rendering.md Offscreen world composition
class WorldRenderLayer final : public ::ergo::render::IRenderLayer {
public:
    WorldRenderLayer(
        adapters::pictor::WorldSceneTargets& targets,
        adapters::pictor::WorldGeometryCache& geometryCache);
    ~WorldRenderLayer() override;

    WorldRenderLayer(const WorldRenderLayer&) = delete;
    WorldRenderLayer& operator=(const WorldRenderLayer&) = delete;

    void initialize(::ergo::render::RenderContext& context) override;
    void set_render_pass(VkRenderPass renderPass) override;
    void record(VkCommandBuffer commandBuffer, VkExtent2D extent) override;
    void shutdown() override;

    // frame ごとに host が publish する。camera の extent は記録時の
    // swapchain extent と一致必須。未 publish のまま `record()` すると、
    // 空の world を描いて「simulation が止まっている」のか「publish 漏れ」
    // なのか判別できなくなるため例外にする。
    void publishFrame(const IsometricCamera& camera, WorldDrawList drawList);

private:
    struct Impl;

    adapters::pictor::WorldSceneTargets* targets_ = nullptr;
    adapters::pictor::WorldGeometryCache* geometryCache_ = nullptr;
    std::unique_ptr<Impl> impl_;
};

}  // namespace konbini::render
