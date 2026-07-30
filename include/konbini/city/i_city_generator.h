#pragma once

#include <memory>

#include "konbini/city/facility_geometry.h"
#include "konbini/sim/entity_id.h"

// @implements spec/interface/figmentum-city-generation.md Required game-side boundary

namespace konbini::city {

// Figmentum adapter を game domain から隔離する境界。この header は
// Figmentum / Pictor の型を一切公開せず、game-owned manifest と geometry
// だけを受け渡す。
// @implements spec/interface/figmentum-city-generation.md Required game-side boundary
class ICityGenerator {
public:
    virtual ~ICityGenerator() = default;

    [[nodiscard]] virtual CityManifest planFirstPlayableCity(
        sim::GenerationalIdPool<sim::FacilityId>& facilityIds) const = 0;

    [[nodiscard]] virtual std::shared_ptr<const FacilityGeometry> buildFacility(
        const ManifestFacility& facility) const = 0;

    // First-playable composition must use this atomic path so a geometry
    // failure cannot consume ids from the caller-owned pool.
    [[nodiscard]] virtual GeneratedCity generateFirstPlayableCity(
        sim::GenerationalIdPool<sim::FacilityId>& facilityIds) const = 0;
};

}  // namespace konbini::city
