#pragma once

#include <cstddef>
#include <vector>

// Ergo's Vulkan forward header can include Win32 before Pictor parses
// ObjectFlags::TRANSPARENT and PassType::OPAQUE.
#ifndef NOGDI
#define NOGDI
#endif
#include "ergo/render/render_layer.h"

// @implements spec/interface/ergo-runtime.md Render host

namespace konbini::adapters::ergo {

// 初期化済み layer を登録順に覚え、失敗時に逆順で解放する owner。
//
// pinned Ergo (771b027f) の `FrameComposer::initialize()` は全 layer の
// `initialize()` が終わってから `initialized_` を立てるため、途中の layer が
// 例外を投げると composer は「未初期化」のまま残る。その状態では destructor
// の `shutdown()` が no-op になり、既に GPU resource を確保した layer が
// 解放されない。upstream の typed result 追加までは、game 側でこの scope を
// 持って rollback する。
//
// 登録される layer は借用で、この scope より長生きしなければならない。
class LayerInitializationScope {
public:
    LayerInitializationScope() = default;
    ~LayerInitializationScope();

    LayerInitializationScope(const LayerInitializationScope&) = delete;
    LayerInitializationScope& operator=(const LayerInitializationScope&) =
        delete;

    // layer の `initialize()` が成功した直後に呼ぶ。
    void onInitialized(::ergo::render::IRenderLayer& layer);

    // layer が自分で `shutdown()` された (composer 経由を含む) ときに呼ぶ。
    // 登録から外れるので rollback で二重 shutdown しない。
    void onShutdown(::ergo::render::IRenderLayer& layer) noexcept;

    // 登録の逆順に `shutdown()` する。個々の shutdown が投げても残りの
    // 解放を続ける (1 つの失敗で他の GPU resource を道連れにしない)。
    void rollback() noexcept;

    // 正常初期化が完了したときに呼ぶ。以後の解放は composer の
    // `shutdown()` が担うので、scope は追跡を手放す。
    void release() noexcept;

    [[nodiscard]] std::size_t initializedCount() const noexcept;

private:
    std::vector<::ergo::render::IRenderLayer*> initialized_;
};

}  // namespace konbini::adapters::ergo
