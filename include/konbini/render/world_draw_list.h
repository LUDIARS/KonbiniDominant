#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "konbini/render/selection_overlay_geometry.h"
#include "konbini/render/store_marker_geometry.h"
#include "konbini/render/world_mesh.h"
#include "konbini/render/world_palette.h"
#include "konbini/render/zoc_overlay_geometry.h"
#include "konbini/sim/render_snapshot.h"

// @implements spec/interface/pictor-rendering.md Offscreen world composition

namespace konbini::render {

// cache 済み GPU geometry を facility key で引き、tint を掛けて 1 回 draw
// する指示。geometry は state に依らず不変で、色だけが tint で変わる。
struct WorldFacilityDraw {
    sim::FigmentumFacilityKey figmentumKey{};
    sim::FacilityId facilityId{};
    WorldColor tint{1.0F, 1.0F, 1.0F, 1.0F};
};

struct WorldDrawListSpec {
    StoreMarkerSpec storeMarker;
    SelectionOverlaySpec selection;
    std::uint32_t zocSegmentCount = 48;
    float zocGroundYMeters = 0.03F;
};

[[nodiscard]] WorldDrawListSpec defaultWorldDrawListSpec() noexcept;

// Record baseFacilities -> storeMesh -> overlayFacilities -> overlayMesh.
// Storefronts write depth so their roof, glazing and lettering self-occlude.
//
// base pass は blend を持たないので、palette の alpha が 1 未満になる
// facility (現状は Destroyed) を base へ入れると半透明指定が無視され不透明に
// 見える。そのため alpha < 1 の facility だけを `overlayFacilities` へ回し、
// depth test あり / depth write なしの overlay pipeline で合成する。
// 「破壊済み facility をどう描くか」の正本はこの分類。
struct WorldDrawList {
    std::vector<WorldFacilityDraw> baseFacilities;
    std::vector<WorldFacilityDraw> overlayFacilities;
    WorldMesh storeMesh;
    WorldMesh overlayMesh;
};

// `overlayMesh` は ZOC → selection の固定順で結合する。
// snapshot 内の順序をそのまま使うので、同じ snapshot からは常に同じ
// geometry が出る。`selectedFacility` が snapshot に無い ID を指したときは
// 無言で選択を捨てず `std::invalid_argument`。
//
// `Replaced` facility は店舗 mesh が同じ footprint を占めるため draw を出さ
// ない。`selectedFacility` の解決は skip より前に行うので、`Replaced` を指す
// 選択も従来どおり検証され、存在しない ID だけが例外になる。
[[nodiscard]] WorldDrawList buildWorldDrawList(
    const sim::RenderSnapshot& snapshot,
    std::optional<sim::FacilityId> selectedFacility,
    const WorldDrawListSpec& spec);

}  // namespace konbini::render
