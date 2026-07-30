#pragma once

#include <cstdint>

#include "konbini/city/city_manifest.h"
#include "konbini/sim/facility_table.h"

// @implements spec/data/world-state.md `FacilityTable`
// @implements spec/interface/figmentum-city-generation.md Required game-side boundary

namespace konbini::city {

// manifest (city 所有の semantic model) から gameplay 側の dense table を作る
// 一方向 projection。table 側の変更は manifest へ書き戻さない。
// @implements spec/data/world-state.md `FacilityTable`
[[nodiscard]] sim::FacilityTable projectFacilityTable(
    const CityManifest& manifest, std::uint32_t dimension = 0);

}  // namespace konbini::city
