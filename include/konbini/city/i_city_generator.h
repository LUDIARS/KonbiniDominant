#pragma once

#include <memory>

#include "konbini/city/facility_geometry.h"
#include "konbini/sim/entity_id.h"

// @implements spec/interface/figmentum-city-generation.md Required game-side boundary

namespace konbini::city {

// Figmentum adapter を game domain から隔離する境界。実装は後続 task で
// 差し込むので、この header は Figmentum / Pictor の型を一切公開しない。
// @implements spec/interface/figmentum-city-generation.md Required game-side boundary
class ICityGenerator {
public:
    virtual ~ICityGenerator() = default;

    [[nodiscard]] virtual CityManifest planFirstPlayableCity(
        sim::GenerationalIdPool<sim::FacilityId>& facilityIds) const = 0;

    [[nodiscard]] virtual std::shared_ptr<const FacilityGeometry> buildFacility(
        const ManifestFacility& facility) const = 0;
};

}  // namespace konbini::city
