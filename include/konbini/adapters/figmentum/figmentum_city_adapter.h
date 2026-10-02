#pragma once

#include <cstddef>

#include "konbini/city/facility_geometry_cache.h"
#include "konbini/city/i_city_generator.h"

// @implements spec/interface/figmentum-city-generation.md Required game-side boundary
// @implements spec/interface/figmentum-city-generation.md Geometry generation

namespace konbini::adapters::figmentum {

class FigmentumCityAdapter final : public city::ICityGenerator {
public:
    // `meshDetail` is the polygonize level every facility is meshed at; a
    // resolution below 1 is rejected when a facility is built.
    explicit FigmentumCityAdapter(
        city::FacilityMeshDetail meshDetail =
            city::kFirstPlayableFacilityMeshDetail) noexcept;

    [[nodiscard]] city::CityManifest planFirstPlayableCity(
        sim::GenerationalIdPool<sim::FacilityId>& facilityIds) const override;

    [[nodiscard]] std::shared_ptr<const city::FacilityGeometry> buildFacility(
        const city::ManifestFacility& facility) const override;

    [[nodiscard]] city::GeneratedCity generateFirstPlayableCity(
        sim::GenerationalIdPool<sim::FacilityId>& facilityIds) const override;

    [[nodiscard]] city::FacilityMeshDetail meshDetail() const noexcept;

    // Memory-pressure response: drops regenerable CPU geometry per `scope`.
    // Returns the number of dropped entries; a dropped recipe re-polygonizes
    // on the next `buildFacility` miss, never inside the frame loop.
    // @implements spec/interface/mobile-platform.md Assets and generated geometry
    std::size_t evictDerivedGeometry(city::GeometryEvictionScope scope) const;

private:
    city::FacilityMeshDetail meshDetail_;
    mutable city::FacilityGeometryCache geometryCache_;
};

}  // namespace konbini::adapters::figmentum
