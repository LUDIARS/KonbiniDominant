#include "konbini/city/city_manifest.h"

#include <cstddef>
#include <set>
#include <stdexcept>

#include "konbini/city/city_manifest_canonical.h"
#include "konbini/city/grid_town.h"

// @implements spec/interface/figmentum-city-generation.md Error contract

namespace konbini::city {

namespace {

// 境界一致は含有として扱う。lot 境界にちょうど接する facility を city 外と
// 判定すると、正しい plan を拒否してしまう。
bool contains(const sim::Bounds3& outer, const sim::Bounds3& inner) noexcept {
    return outer.min.x <= inner.min.x && outer.min.y <= inner.min.y &&
           outer.min.z <= inner.min.z && outer.max.x >= inner.max.x &&
           outer.max.y >= inner.max.y && outer.max.z >= inner.max.z;
}

}  // namespace

// 不正な plan は placeholder で続行させず即 throw する
// (figmentum-city-generation.md#Error contract)。
// @implements spec/interface/figmentum-city-generation.md Error contract
void validateCityManifest(const CityManifest& manifest) {
    if (manifest.schemaVersion != kCityManifestSchemaVersion) {
        throw std::invalid_argument("unsupported CityManifest schema version");
    }
    if (manifest.canonicalVersion != kCityManifestCanonicalVersion ||
        manifest.canonicalHash == 0) {
        throw std::invalid_argument(
            "CityManifest canonical serialization version/hash is invalid");
    }
    if (manifest.generatorSchemaVersion !=
            kSupportedFigmentumPlanSchemaVersion ||
        manifest.generatorRecipeVersion !=
            kSupportedFigmentumRecipeVersion ||
        (manifest.generatorRevision != kFigmentumRevision && !isGridTown(manifest))) {
        throw std::invalid_argument("unsupported Figmentum city-plan revision");
    }
    if (manifest.seed != kFirstPlayableWorldSeed) {
        throw std::invalid_argument("unexpected first-playable city seed");
    }
    if (!sim::isFinite(manifest.stationAnchorMeters) ||
        manifest.stationAnchorMeters.x != 0.0 ||
        manifest.stationAnchorMeters.y != 0.0 ||
        manifest.stationAnchorMeters.z != 0.0) {
        throw std::invalid_argument(
            "first-playable station anchor must be finite and at the origin");
    }
    if (!sim::isFiniteAndOrdered(manifest.boundsMeters)) {
        throw std::invalid_argument("CityManifest bounds are invalid");
    }
    if (manifest.facilities.size() < 3) {
        throw std::invalid_argument(
            "CityManifest requires one station and multiple non-station facilities");
    }

    std::size_t stationCount = 0;
    sim::FigmentumFacilityKey previousKey;
    // index だけでなく generation も含めて重複判定する。`EntityId` は
    // defaulted `<=>` を持つのでそのまま set の key にできる。
    std::set<sim::EntityId> gameIds;
    for (const ManifestFacility& facility : manifest.facilities) {
        if (!facility.id.isValid() || !facility.figmentumKey.isValid()) {
            throw std::invalid_argument("CityManifest contains an invalid facility id");
        }
        if (!gameIds.emplace(facility.id.value()).second) {
            throw std::invalid_argument(
                "CityManifest contains duplicate game facility ids");
        }
        if (previousKey.isValid() &&
            facility.figmentumKey.value() <= previousKey.value()) {
            throw std::invalid_argument(
                "CityManifest facilities must be ordered by Figmentum key");
        }
        previousKey = facility.figmentumKey;

        if (!sim::isFiniteAndOrdered(facility.boundsMeters) ||
            !contains(manifest.boundsMeters, facility.boundsMeters)) {
            throw std::invalid_argument(
                "CityManifest facility bounds are invalid or outside the city");
        }
        if (facility.recipe.kind.empty() || facility.recipe.roof.empty() ||
            !sim::isFinite(facility.recipe.originMeters) ||
            facility.recipe.footprintHalfXMeters <= 0.0 ||
            facility.recipe.footprintHalfZMeters <= 0.0 ||
            facility.recipe.heightMeters <= 0.0 ||
            facility.recipe.blendMeters < 0.0 ||
            facility.recipe.roofBlendMeters < 0.0 ||
            facility.recipe.roofSteps <= 0 || facility.recipe.seed == 0) {
            throw std::invalid_argument(
                "CityManifest contains an invalid building recipe");
        }

        if (facility.role == FacilityRole::Station) {
            ++stationCount;
            if (facility.isBuildable || facility.figmentumKey.value() != 1 ||
                facility.recipe.originMeters.x != manifest.stationAnchorMeters.x ||
                facility.recipe.originMeters.z != manifest.stationAnchorMeters.z) {
                throw std::invalid_argument(
                    "station must be protected, key 1, and located at the anchor");
            }
        } else if (!facility.isBuildable) {
            throw std::invalid_argument(
                "all non-station first-playable facilities must be buildable");
        }
    }
    if (isGridTown(manifest)) {
        if (stationCount != 0) throw std::invalid_argument("grid town has no station building");
        validateGridTownCells(manifest);
    } else if (stationCount != 1) {
        throw std::invalid_argument(
            "CityManifest must contain exactly one station facility");
    }
    if (manifest.canonicalHash != cityManifestCanonicalHash(manifest)) {
        throw std::invalid_argument("CityManifest canonical hash mismatch");
    }
}

}  // namespace konbini::city
