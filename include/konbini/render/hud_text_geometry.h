#pragma once

#include <span>
#include <string>
#include <string_view>

#include "konbini/render/viewport_extent.h"
#include "konbini/render/world_mesh.h"
#include "konbini/render/world_vertex.h"

// @implements spec/feature/ui-ux.md Common HUD
// @implements spec/interface/pictor-rendering.md Game-owned render domain

namespace konbini::render {

// HUD geometry は pixel 空間で組み、NDC への変換は shader 側の push constant
// (viewport size) が行う。CPU 側で NDC へ焼くと、resize のたびに文字が伸びる
// か、extent ごとに geometry を組み直す必要が出る。
//
// `panelColor` の alpha が 0 の場合、背景 quad は生成しない (透明な quad を
// blend で描くと、見えないのに fill rate だけ消費する)。
struct HudTextStyle {
    float glyphPixelScale = 3.0F;
    float glyphSpacingPixels = 1.0F;
    float linePaddingPixels = 4.0F;
    float originXPixels = 16.0F;
    float originYPixels = 16.0F;
    float panelPaddingPixels = 8.0F;
    WorldVertex::ColorRgba textColor{1.0F, 1.0F, 1.0F, 1.0F};
    WorldVertex::ColorRgba panelColor{0.0F, 0.0F, 0.0F, 0.55F};
};

[[nodiscard]] HudTextStyle defaultHudTextStyle() noexcept;

// 1 行分の advance と、行数から決まる block 高さ。layer / test が同じ計算を
// 再実装しないための公開ヘルパー。
[[nodiscard]] float hudLineHeightPixels(const HudTextStyle& style);
[[nodiscard]] float hudTextWidthPixels(
    std::string_view line, const HudTextStyle& style);

// `lines` を pixel 空間の quad mesh へ展開する。原点は左上 (Y 下向き) で、
// Vulkan の framebuffer 座標と一致させる。
//
// font が持たない文字、非正の scale、zero extent はすべて
// `std::invalid_argument`。表示できない文字を空白へ落とすと、HUD が壊れて
// いることに気付けなくなる。
[[nodiscard]] WorldMesh buildHudTextMesh(
    std::span<const std::string> lines, const HudTextStyle& style,
    ViewportExtent extent);

}  // namespace konbini::render
