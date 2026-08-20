#pragma once

#include <cstdint>
#include <memory>

#include "konbini/adapters/ergo/swapchain_identity.h"
#include "konbini/render/viewport_extent.h"

namespace konbini::adapters::pictor {
class WorldGeometryCache;
}

namespace konbini::render {
class HudOverlayLayer;
class WorldCompositeLayer;
class WorldRenderLayer;
}  // namespace konbini::render

// @implements spec/interface/pictor-rendering.md Offscreen world composition
// @implements spec/interface/ergo-runtime.md Render host

namespace konbini::adapters::ergo {

class RenderDeviceHost;

// offscreen world pass -> barrier -> swapchain composite -> HUD の 2 pass 構成
// と、その pass 列を保持する `FrameComposer` の所有者。
//
// pinned Ergo (771b027f) の `FrameComposer` は `add_pass()` 時の
// `VkRenderPass` を後から差し替えられないため、swapchain 再生成では composer
// ごと作り直す。layer は composer の `shutdown()` で逆順に解放され、
// scene target を作り直してから同じ順で再初期化する。
class WorldFrameGraph {
public:
    WorldFrameGraph();
    ~WorldFrameGraph();

    WorldFrameGraph(const WorldFrameGraph&) = delete;
    WorldFrameGraph& operator=(const WorldFrameGraph&) = delete;

    // `host` は借用で、この graph より長生きすること。失敗時は確保済み
    // resource を逆順で解放してから例外を投げる。
    void initialize(RenderDeviceHost& host);
    void shutdown() noexcept;

    [[nodiscard]] bool isInitialized() const noexcept;

    // 都市 geometry の upload 先。startup で 1 回だけ載せる。
    [[nodiscard]] pictor::WorldGeometryCache& geometryCache();

    [[nodiscard]] render::WorldRenderLayer& worldLayer();
    [[nodiscard]] render::HudOverlayLayer& hudLayer();

    // 現在の scene target extent。camera と picker はこの extent を使う。
    [[nodiscard]] render::ViewportExtent extent() const;

    // 1 frame 記録して present する。swapchain の out-of-date と device /
    // surface lost を分類し、再生成が起きていれば依存 resource を作り直す。
    // 致命 (`isFatal`) の場合も例外は投げず、判断は host のループへ返す。
    [[nodiscard]] FrameOutcome runFrame(float deltaSeconds);

    // 最小化などで再構築を保留した状態か。
    [[nodiscard]] bool rebuildPending() const noexcept;

    // window size が swapchain と食い違ったときに host から要求する明示的な
    // 再構築。次の `runFrame()` は現在 publish 済みの frame を記録せず、
    // composer / layer を先に解放して Pictor の swapchain を明示再生成する。
    // 呼び出し元は次の app-loop で新しい extent に対して再 publish する。
    void requestRebuild() noexcept;

    [[nodiscard]] std::uint64_t rebuildCount() const noexcept;

private:
    struct Impl;

    void buildComposer();
    void resetComposer();
    void finishRebuild();
    void rebuild();
    [[nodiscard]] bool recreateSwapchainAndRebuild();

    std::unique_ptr<Impl> impl_;
};

}  // namespace konbini::adapters::ergo
