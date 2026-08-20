#pragma once

#include "konbini/render/world_mesh.h"
#include "konbini/sim/render_snapshot.h"

// @implements spec/interface/pictor-rendering.md Game-owned render domain

namespace konbini::render {

// 選択中 facility の bounds を少しだけ膨らませた枠。膨張させないと base
// geometry と同一面で z-fighting するため、margin は 0 より大きい値のみ。
struct SelectionOverlaySpec {
    double marginMeters = 0.35;
    float alpha = 0.35F;
};

[[nodiscard]] SelectionOverlaySpec defaultSelectionOverlaySpec() noexcept;

// `facility.boundsMeters` は `isFiniteAndOrdered` を満たす必要がある。退化
// bounds を持つ facility を選択状態にしたまま無言で描画を省くと、選択が
// 効いていないのか描けていないのか区別できないので落とす。
[[nodiscard]] WorldMesh buildSelectionOverlayGeometry(
    const sim::RenderFacility& facility, const SelectionOverlaySpec& spec);

}  // namespace konbini::render
