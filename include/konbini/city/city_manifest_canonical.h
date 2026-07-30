#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "konbini/city/city_manifest.h"

// @implements spec/interface/figmentum-city-generation.md Output: `CityManifest`

namespace konbini::city {

// 入力順に依存しない byte 列を返す。同じ都市が同じ hash になることが
// generator revision / seed の同一性判定の前提になる。
// @implements spec/interface/figmentum-city-generation.md Output: `CityManifest`
[[nodiscard]] std::vector<std::byte> serializeCityManifestCanonical(
    const CityManifest& manifest);

// 非暗号学的 hash (FNV-1a)。生成物の同一性・取り違え検出用であり、
// 改竄検出には使えない。外部由来の manifest を信頼する用途へ広げない。
// @implements spec/interface/figmentum-city-generation.md Error contract
[[nodiscard]] std::uint64_t cityManifestCanonicalHash(
    const CityManifest& manifest);

}  // namespace konbini::city
