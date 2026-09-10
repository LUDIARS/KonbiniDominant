#pragma once

#include <span>

#include "konbini/render/world_mesh.h"
#include "konbini/sim/render_snapshot.h"

// @implements spec/interface/pictor-rendering.md Game-owned render domain

namespace konbini::render {

// Three original storefronts, scaled to the placement footprint. Presentation
// only: legacy chain IDs and gameplay economics remain stable.
struct StoreMarkerSpec {
    double halfWidthMeters = 3.0;
    double heightMeters = 3.6;
    float alpha = 1.0F;
    double gridCellMeters = 0.0;
};

[[nodiscard]] StoreMarkerSpec defaultStoreMarkerSpec() noexcept;

// snapshot の store 順をそのまま使い、店舗ごとに外観を積む。無効な store
// (ID 未設定、非有限座標、未知 chain) は飛ばさず `std::invalid_argument`。
[[nodiscard]] WorldMesh buildStoreMarkerGeometry(
    std::span<const sim::RenderStore> stores, const StoreMarkerSpec& spec);

}  // namespace konbini::render
