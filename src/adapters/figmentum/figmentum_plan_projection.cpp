#include "figmentum_plan_projection.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "figmentum/gen/building.h"

#include "figmentum_conversions.h"
#include "konbini/city/city_manifest_canonical.h"
#include "konbini/sim/entity_id.h"

namespace konbini::adapters::figmentum {

namespace {

// @implements spec/interface/figmentum-city-generation.md Output: `CityManifest`
city::FacilityRole convert(const fg::FacilityRole role) {
    switch (role) {
        case fg::FacilityRole::Station:
            return city::FacilityRole::Station;
        case fg::FacilityRole::Residential:
            return city::FacilityRole::Residential;
        case fg::FacilityRole::Commercial:
            return city::FacilityRole::Commercial;
        case fg::FacilityRole::Office:
            return city::FacilityRole::Office;
        case fg::FacilityRole::Civic:
            return city::FacilityRole::Civic;
        case fg::FacilityRole::Utility:
            return city::FacilityRole::Utility;
    }
    throw std::logic_error("unknown Figmentum facility role");
}

// @implements spec/interface/figmentum-city-generation.md Output: `CityManifest`
city::BuildingRecipe convert(const fg::BuildingParams& recipe) {
    return {
        .kind = fg::buildingKindName(recipe.kind),
        .roof = fg::roofKindName(recipe.roof),
        .originMeters = toVec3(recipe.origin),
        .footprintHalfXMeters = static_cast<double>(recipe.footprintX),
        .footprintHalfZMeters = static_cast<double>(recipe.footprintZ),
        .heightMeters = static_cast<double>(recipe.height),
        .hasRooftopProps = recipe.rooftopProps,
        .blendMeters = static_cast<double>(recipe.blend),
        .roofBlendMeters = static_cast<double>(recipe.roofBlend),
        .roofSteps = recipe.roofSteps,
        .seed = recipe.seed,
    };
}

// @implements spec/interface/figmentum-city-generation.md Error contract
bool equal(const fg::Aabb& left, const fg::Aabb& right) noexcept {
    return left.min.x == right.min.x && left.min.y == right.min.y &&
           left.min.z == right.min.z && left.max.x == right.max.x &&
           left.max.y == right.max.y && left.max.z == right.max.z;
}

// @implements spec/interface/figmentum-city-generation.md Error contract
void validatePlan(const fg::CityPlan& plan) {
    if (plan.schemaVersion != fg::kCityPlanSchemaVersion ||
        plan.schemaVersion != city::kSupportedFigmentumPlanSchemaVersion ||
        plan.recipeVersion != fg::kCityPlanRecipeVersion ||
        plan.recipeVersion != city::kSupportedFigmentumRecipeVersion ||
        plan.seed != city::kFirstPlayableWorldSeed) {
        throw std::runtime_error("Figmentum returned an unsupported city-plan version");
    }
    if (!std::isfinite(plan.stationAnchor.x) ||
        !std::isfinite(plan.stationAnchor.y) ||
        !std::isfinite(plan.stationAnchor.z) || plan.stationAnchor.x != 0.0f ||
        plan.stationAnchor.y != 0.0f || plan.stationAnchor.z != 0.0f) {
        throw std::runtime_error("Figmentum returned an invalid station anchor");
    }
    if (plan.facilities.size() < 3) {
        throw std::runtime_error(
            "Figmentum returned fewer than two non-station facilities");
    }

    std::size_t stationCount = 0;
    fg::FacilityId previousId = 0;
    for (const fg::FacilityPlan& facility : plan.facilities) {
        if (facility.id == 0 || facility.id <= previousId) {
            throw std::runtime_error(
                "Figmentum facility ids are invalid or not strictly ordered");
        }
        previousId = facility.id;
        if (facility.role == fg::FacilityRole::Station) {
            ++stationCount;
            if (facility.id != fg::kStationFacilityId ||
                facility.building.origin.x != plan.stationAnchor.x ||
                facility.building.origin.z != plan.stationAnchor.z) {
                throw std::runtime_error(
                    "Figmentum station identity or anchor is invalid");
            }
        }
        if (!equal(facility.bounds, fg::buildingBounds(facility.building))) {
            throw std::runtime_error(
                "Figmentum facility bounds do not match its building recipe");
        }
        if (!sim::isFiniteAndOrdered(toBounds3(facility.bounds))) {
            throw std::runtime_error(
                "Figmentum facility bounds are non-finite or degenerate");
        }
    }
    if (stationCount != 1) {
        throw std::runtime_error(
            "Figmentum city plan must contain exactly one station");
    }
}

// @implements spec/interface/figmentum-city-generation.md Output: `CityManifest`
sim::Bounds3 aggregateBounds(const fg::CityPlan& plan) {
    sim::Bounds3 bounds = toBounds3(plan.facilities.front().bounds);
    for (const fg::FacilityPlan& facility : plan.facilities) {
        const sim::Bounds3 candidate = toBounds3(facility.bounds);
        bounds.min.x = std::min(bounds.min.x, candidate.min.x);
        bounds.min.y = std::min(bounds.min.y, candidate.min.y);
        bounds.min.z = std::min(bounds.min.z, candidate.min.z);
        bounds.max.x = std::max(bounds.max.x, candidate.max.x);
        bounds.max.y = std::max(bounds.max.y, candidate.max.y);
        bounds.max.z = std::max(bounds.max.z, candidate.max.z);
    }
    return bounds;
}

}  // namespace

// @implements spec/interface/figmentum-city-generation.md Output: `CityManifest`
// @implements spec/interface/figmentum-city-generation.md Required game-side boundary
city::CityManifest projectCityPlan(
    const fg::CityPlan& plan,
    sim::GenerationalIdPool<sim::FacilityId>& facilityIds) {
    validatePlan(plan);

    sim::GenerationalIdPool<sim::FacilityId> stagedFacilityIds = facilityIds;
    city::CityManifest manifest{
        .schemaVersion = city::kCityManifestSchemaVersion,
        .canonicalVersion = city::kCityManifestCanonicalVersion,
        .generatorSchemaVersion = plan.schemaVersion,
        .generatorRecipeVersion = plan.recipeVersion,
        .generatorRevision = std::string(city::kFigmentumRevision),
        .seed = plan.seed,
        .stationAnchorMeters = toVec3(plan.stationAnchor),
        .boundsMeters = aggregateBounds(plan),
    };
    manifest.facilities.reserve(plan.facilities.size());
    for (const fg::FacilityPlan& facility : plan.facilities) {
        const city::FacilityRole role = convert(facility.role);
        manifest.facilities.push_back({
            .id = stagedFacilityIds.acquire(),
            .figmentumKey =
                sim::FigmentumFacilityKey{.rawValue = facility.id},
            .cell = {.x = facility.cell.x,
                     .z = facility.cell.z,
                     .lotSlot = facility.lotSlot},
            .role = role,
            .recipe = convert(facility.building),
            .boundsMeters = toBounds3(facility.bounds),
            .isBuildable = role != city::FacilityRole::Station,
        });
    }
    manifest.canonicalHash = city::cityManifestCanonicalHash(manifest);
    city::validateCityManifest(manifest);
    static_assert(std::is_nothrow_move_assignable_v<
                      sim::GenerationalIdPool<sim::FacilityId>>,
                  "facility id-pool commit must not throw");
    facilityIds = std::move(stagedFacilityIds);
    return manifest;
}

}  // namespace konbini::adapters::figmentum
