#pragma once

#include "konbini/city/facility_geometry.h"
#include "konbini/render/world_mesh.h"

// @implements spec/interface/pictor-rendering.md Geometry conversion

namespace konbini::render {

// facility の presentation 色は state (Intact / Replaced / Destroyed) と
// buildable で毎 tick 変わりうる一方、geometry 自体は Figmentum recipe だけで
// 決まる。色を vertex へ焼くと state 変化のたびに GPU buffer を作り直す必要が
// 出るため、conversion では中立色を書き込み、実際の色は draw ごとの tint で
// 与える。`WorldDrawList` がその tint の正本。
inline constexpr WorldVertex::ColorRgba kFacilityNeutralVertexColor{
    1.0F, 1.0F, 1.0F, 1.0F};

// `city::FacilityGeometry` を game-owned な `WorldMesh` へ射影する。
//
// Figmentum 側の値は信用せず入口で検証する: position / normal の非有限値、
// position と normal の要素数不一致、空 mesh、3 の倍数でない index 数、
// vertex 範囲外の index はすべて `std::invalid_argument`。縮退 normal は
// 0 ベクトルとして通し、shader 側が陰影を掛けない扱いにする。
[[nodiscard]] WorldMesh buildFacilityWorldMesh(
    const city::FacilityGeometry& geometry);

}  // namespace konbini::render
