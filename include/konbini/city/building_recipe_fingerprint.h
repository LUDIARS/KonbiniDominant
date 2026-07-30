#pragma once

#include <cstdint>

#include "konbini/city/city_manifest.h"

// @implements spec/interface/figmentum-city-generation.md Geometry generation

namespace konbini::city {

inline constexpr std::uint32_t kBuildingRecipeFingerprintVersion = 1;

[[nodiscard]] std::uint64_t buildingRecipeFingerprint(
    const BuildingRecipe& recipe);

}  // namespace konbini::city
