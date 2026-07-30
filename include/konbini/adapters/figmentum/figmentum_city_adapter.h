#pragma once

#include "konbini/city/facility_geometry_cache.h"
#include "konbini/city/i_city_generator.h"

// @implements spec/interface/figmentum-city-generation.md Required game-side boundary
// @implements spec/interface/figmentum-city-generation.md Geometry generation

namespace konbini::adapters::figmentum {

class FigmentumCityAdapter final : public city::ICityGenerator {
public:
    [[nodiscard]] city::CityManifest planFirstPlayableCity(
        sim::GenerationalIdPool<sim::FacilityId>& facilityIds) const override;

    [[nodiscard]] std::shared_ptr<const city::FacilityGeometry> buildFacility(
        const city::ManifestFacility& facility) const override;

    [[nodiscard]] city::GeneratedCity generateFirstPlayableCity(
        sim::GenerationalIdPool<sim::FacilityId>& facilityIds) const override;

private:
    mutable city::FacilityGeometryCache geometryCache_;
};

}  // namespace konbini::adapters::figmentum
