#include "konbini/adapters/figmentum/figmentum_city_adapter.h"

#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "figmentum/gen/city_plan.h"

#include "figmentum_facility_mesher.h"
#include "figmentum_plan_projection.h"
#include "konbini/city/city_manifest.h"

namespace konbini::adapters::figmentum {

namespace {

// @implements spec/interface/figmentum-city-generation.md Geometry generation
std::shared_ptr<const city::FacilityGeometry> bindToFacility(
    std::shared_ptr<const city::FacilityGeometry> geometry,
    const sim::FigmentumFacilityKey facilityKey) {
    if (geometry->figmentumKey == facilityKey) {
        return geometry;
    }

    // Recipe geometry is cacheable across facilities, but identity is not.
    // Return a geometry value with the caller's stable facility key.
    auto rebound = std::make_shared<city::FacilityGeometry>(*geometry);
    rebound->figmentumKey = facilityKey;
    return rebound;
}

}  // namespace

// @implements spec/interface/figmentum-city-generation.md Required game-side boundary
city::CityManifest FigmentumCityAdapter::planFirstPlayableCity(
    sim::GenerationalIdPool<sim::FacilityId>& facilityIds) const {
    const fg::CityPlan plan =
        fg::planCity(fg::CityPlanParams{}, city::kFirstPlayableWorldSeed);
    return projectCityPlan(plan, facilityIds);
}

// @implements spec/interface/figmentum-city-generation.md Geometry generation
std::shared_ptr<const city::FacilityGeometry>
FigmentumCityAdapter::buildFacility(
    const city::ManifestFacility& facility) const {
    const PreparedFacilityGeometry prepared =
        prepareFacilityGeometry(facility);
    if (const auto cached = geometryCache_.find(prepared.cacheKey);
        cached != nullptr) {
        return bindToFacility(cached, facility.figmentumKey);
    }
    auto geometry =
        std::make_shared<const city::FacilityGeometry>(
            generateFacilityGeometry(prepared));
    if (geometry->cacheKey != prepared.cacheKey) {
        throw std::logic_error(
            "Figmentum facility mesher returned an inconsistent cache key");
    }
    // Another worker may win an equal-recipe insert between find() and here.
    // Rebind the canonical cache entry just as we do for a direct cache hit.
    return bindToFacility(geometryCache_.insert(std::move(geometry)),
                          facility.figmentumKey);
}

// @implements spec/interface/figmentum-city-generation.md Required game-side boundary
city::GeneratedCity FigmentumCityAdapter::generateFirstPlayableCity(
    sim::GenerationalIdPool<sim::FacilityId>& facilityIds) const {
    sim::GenerationalIdPool<sim::FacilityId> stagedFacilityIds = facilityIds;
    city::GeneratedCity result;
    result.manifest = planFirstPlayableCity(stagedFacilityIds);
    result.geometry.reserve(result.manifest.facilities.size());
    for (const city::ManifestFacility& facility :
         result.manifest.facilities) {
        result.geometry.push_back(buildFacility(facility));
    }
    static_assert(std::is_nothrow_move_assignable_v<
                      sim::GenerationalIdPool<sim::FacilityId>>,
                  "facility id-pool commit must not throw");
    facilityIds = std::move(stagedFacilityIds);
    return result;
}

}  // namespace konbini::adapters::figmentum
