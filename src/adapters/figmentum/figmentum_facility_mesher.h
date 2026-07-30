#pragma once

#include "figmentum/gen/building.h"

#include "konbini/city/facility_geometry.h"

namespace konbini::adapters::figmentum {

struct PreparedFacilityGeometry {
    sim::FigmentumFacilityKey figmentumKey{};
    city::FacilityGeometryCacheKey cacheKey;
    fg::BuildingParams recipe;
    fg::Aabb bounds;
};

[[nodiscard]] PreparedFacilityGeometry prepareFacilityGeometry(
    const city::ManifestFacility& facility);

[[nodiscard]] city::FacilityGeometry generateFacilityGeometry(
    const PreparedFacilityGeometry& prepared);

}  // namespace konbini::adapters::figmentum
