#pragma once

#include <stdexcept>

#include "ergo/render/render_backend.h"

namespace ergo::render {
struct RenderContext;
}

// @implements spec/interface/ergo-runtime.md Render readiness
// @implements spec/interface/mobile-platform.md Surface and renderer

namespace konbini::adapters::ergo {

// Ergo が「実描画できない」と型付きで返した結果を、起動経路の明示 error へ
// 変換する。pinned Ergo (7f0d6bbd) の `check_render_requirements()` /
// `FrameComposer::initialize()` は失敗を `RenderBackendError` で返すだけなので、
// 戻り値を捨てると headless 成功へ縮退する。game 側はここを必ず通す。
class RenderUnavailableError : public std::runtime_error {
public:
    RenderUnavailableError(::ergo::render::RenderBackendError reason,
                           const char* stage);

    [[nodiscard]] ::ergo::render::RenderBackendError reason() const noexcept;

private:
    ::ergo::render::RenderBackendError reason_;
};

// このビルドが期待する Ergo render platform。Android / iOS / desktop の
// どれで組まれたかを compile 定義から決める。
[[nodiscard]] ::ergo::render::RenderPlatform expectedRenderPlatform() noexcept;

// pinned Ergo が実描画経路を含み、期待 platform で組まれていることを要求する。
// configure 側の検査 (cmake/RequireErgoRealRender.cmake) と同じ契約の実行時版。
void requireRealRenderBackend(const char* stage);

// `reason` が `None` 以外なら `RenderUnavailableError` を投げる。
void requireRenderReady(::ergo::render::RenderBackendError reason,
                        const char* stage);

// `check_render_requirements(context)` の結果を要求する。
void requireRenderReady(const ::ergo::render::RenderContext& context,
                        const char* stage);

}  // namespace konbini::adapters::ergo
