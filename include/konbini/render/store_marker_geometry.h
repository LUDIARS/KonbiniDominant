#pragma once

#include <span>

#include "konbini/render/world_mesh.h"
#include "konbini/sim/render_snapshot.h"

// @implements spec/interface/pictor-rendering.md Game-owned render domain

namespace konbini::render {

// first playable の店舗マーカーは chain 色の直方体 1 つ。BASE-FP-PALETTE-01
// と同じく presentation 専用の暫定値で、gameplay 分岐には使わない。
struct StoreMarkerSpec {
    double halfWidthMeters = 3.0;
    double heightMeters = 7.0;
    float alpha = 0.85F;
};

[[nodiscard]] StoreMarkerSpec defaultStoreMarkerSpec() noexcept;

// snapshot の store 順をそのまま使い、店舗ごとに 1 box を積む。無効な store
// (ID 未設定、非有限座標、未知 chain) は飛ばさず `std::invalid_argument`。
[[nodiscard]] WorldMesh buildStoreMarkerGeometry(
    std::span<const sim::RenderStore> stores, const StoreMarkerSpec& spec);

}  // namespace konbini::render
