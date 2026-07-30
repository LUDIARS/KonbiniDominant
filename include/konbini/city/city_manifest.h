#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "konbini/sim/entity_id.h"
#include "konbini/sim/figmentum_facility_key.h"
#include "konbini/sim/game_state.h"
#include "konbini/sim/math_types.h"

// @implements spec/interface/figmentum-city-generation.md Output: `CityManifest`
// @implements spec/interface/figmentum-city-generation.md Required game-side boundary

namespace konbini::city {

inline constexpr std::uint32_t kCityManifestSchemaVersion = 1;
inline constexpr std::uint32_t kCityManifestCanonicalVersion = 1;
inline constexpr std::uint32_t kSupportedFigmentumPlanSchemaVersion = 1;
inline constexpr std::uint32_t kSupportedFigmentumRecipeVersion = 1;
// seed の正本は sim 側。city が独自の 42 を持つと、GameState と manifest が
// 別 seed を指したまま validation を通ってしまう。
inline constexpr std::uint64_t kFirstPlayableWorldSeed =
    sim::kFirstPlayableWorldSeed;
// 受け入れ可能な Figmentum の revision を 1 点に固定する。upstream が動くと
// 同じ seed から別 geometry が出るので、revision 不一致は fail-fast させる。
inline constexpr std::string_view kFigmentumRevision =
    "3ee998f487d984f54003c4ec3c4f7ba00b53eec3";

// @implements spec/interface/figmentum-city-generation.md Facility
enum class FacilityRole : std::uint8_t {
    Station = 0,
    Residential,
    Commercial,
    Office,
    Civic,
    Utility,
};

// @implements spec/interface/figmentum-city-generation.md Lot
struct CanonicalCell {
    std::int32_t x = 0;
    std::int32_t z = 0;
    std::uint8_t lotSlot = 0;
};

// @implements spec/interface/figmentum-city-generation.md Input: `NkxiRecipe`
struct BuildingRecipe {
    std::string kind;
    std::string roof;
    sim::Vec3 originMeters{};
    double footprintHalfXMeters = 0.0;
    double footprintHalfZMeters = 0.0;
    double heightMeters = 0.0;
    bool hasRooftopProps = false;
    double blendMeters = 0.0;
    double roofBlendMeters = 0.0;
    std::int32_t roofSteps = 0;
    std::uint32_t seed = 0;
};

// `id` は game が所有する stable ID、`figmentumKey` は generator 側の key。
// 両者を同一視しないため別型で持つ (figmentum-city-generation.md#Required
// game-side boundary)。
// @implements spec/interface/figmentum-city-generation.md Facility
struct ManifestFacility {
    sim::FacilityId id{};
    sim::FigmentumFacilityKey figmentumKey{};
    CanonicalCell cell{};
    FacilityRole role = FacilityRole::Residential;
    BuildingRecipe recipe;
    sim::Bounds3 boundsMeters{};
    bool isBuildable = false;
};

// @implements spec/interface/figmentum-city-generation.md Output: `CityManifest`
struct CityManifest {
    std::uint32_t schemaVersion = kCityManifestSchemaVersion;
    std::uint32_t canonicalVersion = kCityManifestCanonicalVersion;
    std::uint64_t canonicalHash = 0;
    std::uint32_t generatorSchemaVersion = 0;
    std::uint32_t generatorRecipeVersion = 0;
    std::string generatorRevision;
    std::uint64_t seed = 0;
    sim::Vec3 stationAnchorMeters{};
    sim::Bounds3 boundsMeters{};
    std::vector<ManifestFacility> facilities;
};

// @implements spec/interface/figmentum-city-generation.md Error contract
void validateCityManifest(const CityManifest& manifest);

}  // namespace konbini::city
