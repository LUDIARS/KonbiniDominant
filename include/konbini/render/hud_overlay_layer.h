#pragma once

#include <memory>

#include "konbini/render/viewport_extent.h"
#include "konbini/render/world_mesh.h"

// Ergo's Vulkan forward header can include Win32 before Pictor parses
// ObjectFlags::TRANSPARENT and PassType::OPAQUE. Suppress the colliding GDI
// macros at this public boundary, matching Pictor's registry headers.
#ifndef NOGDI
#define NOGDI
#endif
#include "ergo/render/render_layer.h"

namespace konbini::render {

// swapchain pass (pass 1) の HUD 記録担当。`WorldCompositeLayer` が world の
// 合成結果を書いた後に、同じ pass へ alpha 合成で重ねる。
//
// world pass 側と違い scene target を参照しないので、swapchain 再生成では
// pipeline の作り直しだけで済む。ただし記録する render pass は Pictor の
// 既定 pass でなければならない (offscreen world pass へ HUD を書くと、
// composite 前の HDR target へ焼き込まれてしまう)。
//
// @implements spec/feature/ui-ux.md Common HUD
class HudOverlayLayer final : public ::ergo::render::IRenderLayer {
public:
    HudOverlayLayer();
    ~HudOverlayLayer() override;

    HudOverlayLayer(const HudOverlayLayer&) = delete;
    HudOverlayLayer& operator=(const HudOverlayLayer&) = delete;

    void initialize(::ergo::render::RenderContext& context) override;
    void set_render_pass(VkRenderPass renderPass) override;
    void record(VkCommandBuffer commandBuffer, VkExtent2D extent) override;
    void shutdown() override;

    // frame ごとに host が publish する。未 publish のまま `record()` すると
    // 「HUD が空」なのか「publish 漏れ」なのか区別できないので例外にする。
    // 空 mesh (行が 1 つも無い) は publish 済みとして扱い、draw を出さない。
    void publishFrame(WorldMesh mesh, ViewportExtent extent);

private:
    struct Impl;

    std::unique_ptr<Impl> impl_;
};

}  // namespace konbini::render
