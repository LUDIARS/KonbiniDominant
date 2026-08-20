#pragma once

#include "konbini/adapters/ergo/layer_initialization_scope.h"

// @implements spec/interface/ergo-runtime.md Render host

namespace konbini::adapters::ergo {

// `FrameComposer` へ登録する薄い proxy。実 layer への呼び出しをそのまま
// 転送しつつ、`initialize()` / `shutdown()` の成否を
// `LayerInitializationScope` へ知らせる。
//
// composer は layer の初期化進捗を外へ出さないので、途中失敗の rollback 対象
// を game 側が知るにはこの通知が要る。proxy 自身は GPU resource を持たない。
class TrackedRenderLayer final : public ::ergo::render::IRenderLayer {
public:
    // `inner` と `scope` は借用。どちらもこの proxy より長生きすること。
    TrackedRenderLayer(
        ::ergo::render::IRenderLayer& inner, LayerInitializationScope& scope);

    void initialize(::ergo::render::RenderContext& context) override;
    void set_render_pass(VkRenderPass renderPass) override;
    void on_first_frame(::ergo::render::RenderContext& context) override;
    void update(const ::ergo::render::FrameContext& frame) override;
    void record(VkCommandBuffer commandBuffer, VkExtent2D extent) override;
    void shutdown() override;

    [[nodiscard]] ::ergo::render::IRenderLayer& inner() const noexcept;

private:
    ::ergo::render::IRenderLayer* inner_ = nullptr;
    LayerInitializationScope* scope_ = nullptr;
};

}  // namespace konbini::adapters::ergo
