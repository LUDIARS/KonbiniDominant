#pragma once

#include <compare>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "konbini/city/city_manifest.h"
#include "konbini/sim/figmentum_facility_key.h"
#include "konbini/sim/math_types.h"

// @implements spec/interface/figmentum-city-generation.md Geometry generation

namespace konbini::city {

inline constexpr std::int32_t kFirstPlayableFacilityPolygonizeResolution = 24;
inline constexpr std::uint32_t kFacilityGeometryCacheSchemaVersion = 1;
inline constexpr std::uint32_t kFacilityVertexFormatVersion = 1;

// recipe hash だけでなく generator revision / 解像度 / vertex format も key に
// 含める。どれか 1 つでも動くと同じ recipe から別 mesh が出るため、古い
// geometry を再利用してはいけない。
// @implements spec/interface/figmentum-city-generation.md Geometry generation
struct FacilityGeometryCacheKey {
    std::uint32_t schemaVersion = kFacilityGeometryCacheSchemaVersion;
    std::string generatorRevision;
    std::uint64_t recipeHash = 0;
    std::int32_t polygonizeResolution = 0;
    std::uint32_t vertexFormatVersion = kFacilityVertexFormatVersion;

    auto operator<=>(const FacilityGeometryCacheKey&) const = default;
};

// @implements spec/interface/figmentum-city-generation.md Geometry generation
struct FacilityGeometry {
    sim::FigmentumFacilityKey figmentumKey{};
    FacilityGeometryCacheKey cacheKey;
    std::vector<sim::Vec3> positionsMeters;
    std::vector<sim::Vec3> normals;
    std::vector<std::uint32_t> indices;
};

// @implements spec/interface/figmentum-city-generation.md Output: `CityManifest`
struct GeneratedCity {
    CityManifest manifest;
    std::vector<std::shared_ptr<const FacilityGeometry>> geometry;
};

}  // namespace konbini::city
