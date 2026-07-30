#include "figmentum_facility_mesher.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "figmentum/gen/building.h"
#include "figmentum/meshing/marching_cubes.h"

#include "figmentum_conversions.h"
#include "konbini/city/building_recipe_fingerprint.h"

namespace konbini::adapters::figmentum {

namespace {

// @implements spec/interface/figmentum-city-generation.md Error contract
void require(const bool condition, const std::string_view message) {
    if (!condition) {
        throw std::invalid_argument(std::string(message));
    }
}

// @implements spec/interface/figmentum-city-generation.md Error contract
float checkedFloat(const double value, const char* field) {
    const double maximum =
        static_cast<double>(std::numeric_limits<float>::max());
    const std::string prefix = std::string("building recipe ") + field;
    require(std::isfinite(value), prefix + " must be finite");
    require(value >= -maximum,
            prefix + " is below the Figmentum float range");
    require(value <= maximum,
            prefix + " is above the Figmentum float range");
    return static_cast<float>(value);
}

// @implements spec/interface/figmentum-city-generation.md Geometry generation
fg::BuildingParams convert(const city::BuildingRecipe& recipe) {
    require(!recipe.kind.empty(), "building recipe kind is empty");
    require(!recipe.roof.empty(), "building recipe roof is empty");
    require(sim::isFinite(recipe.originMeters),
            "building recipe origin must be finite");
    require(recipe.footprintHalfXMeters > 0.0,
            "building recipe footprintHalfX must be positive");
    require(recipe.footprintHalfZMeters > 0.0,
            "building recipe footprintHalfZ must be positive");
    require(recipe.heightMeters > 0.0,
            "building recipe height must be positive");
    require(recipe.blendMeters >= 0.0,
            "building recipe blend must be non-negative");
    require(recipe.roofBlendMeters >= 0.0,
            "building recipe roofBlend must be non-negative");
    require(recipe.roofSteps > 0,
            "building recipe roofSteps must be positive");
    require(recipe.seed != 0, "building recipe seed must be non-zero");

    fg::BuildingKind kind;
    if (!fg::buildingKindFromName(recipe.kind, kind)) {
        throw std::invalid_argument("unknown Figmentum building kind");
    }
    fg::RoofKind roof;
    if (!fg::roofKindFromName(recipe.roof, roof)) {
        throw std::invalid_argument("unknown Figmentum roof kind");
    }
    return {
        .kind = kind,
        .roof = roof,
        .origin = {checkedFloat(recipe.originMeters.x, "origin.x"),
                   checkedFloat(recipe.originMeters.y, "origin.y"),
                   checkedFloat(recipe.originMeters.z, "origin.z")},
        .footprintX =
            checkedFloat(recipe.footprintHalfXMeters, "footprintHalfX"),
        .footprintZ =
            checkedFloat(recipe.footprintHalfZMeters, "footprintHalfZ"),
        .height = checkedFloat(recipe.heightMeters, "height"),
        .rooftopProps = recipe.hasRooftopProps,
        .blend = checkedFloat(recipe.blendMeters, "blend"),
        .roofBlend = checkedFloat(recipe.roofBlendMeters, "roofBlend"),
        .roofSteps = recipe.roofSteps,
        .seed = recipe.seed,
    };
}

// @implements spec/interface/figmentum-city-generation.md Geometry generation
sim::Vec3 subtract(const sim::Vec3 left, const sim::Vec3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

// @implements spec/interface/figmentum-city-generation.md Geometry generation
sim::Vec3 cross(const sim::Vec3 left, const sim::Vec3 right) noexcept {
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

// @implements spec/interface/figmentum-city-generation.md Geometry generation
void add(sim::Vec3& target, const sim::Vec3 value) noexcept {
    target.x += value.x;
    target.y += value.y;
    target.z += value.z;
}

// @implements spec/interface/figmentum-city-generation.md Geometry generation
std::vector<sim::Vec3> generateNormals(
    const std::vector<sim::Vec3>& positions,
    const std::vector<std::uint32_t>& indices) {
    std::vector<sim::Vec3> accumulated(positions.size());
    for (std::size_t triangle = 0; triangle < indices.size(); triangle += 3) {
        const std::uint32_t indexA = indices[triangle];
        const std::uint32_t indexB = indices[triangle + 1];
        const std::uint32_t indexC = indices[triangle + 2];
        if (indexA >= positions.size() || indexB >= positions.size() ||
            indexC >= positions.size()) {
            throw std::runtime_error(
                "Figmentum polygonize returned an out-of-range index");
        }
        const sim::Vec3 normal =
            cross(subtract(positions[indexB], positions[indexA]),
                  subtract(positions[indexC], positions[indexA]));
        if (!sim::isFinite(normal)) {
            throw std::runtime_error(
                "Figmentum polygonize produced a non-finite triangle");
        }
        add(accumulated[indexA], normal);
        add(accumulated[indexB], normal);
        add(accumulated[indexC], normal);
    }

    for (sim::Vec3& normal : accumulated) {
        const double lengthSquared =
            normal.x * normal.x + normal.y * normal.y + normal.z * normal.z;
        if (!std::isfinite(lengthSquared) || lengthSquared <= 0.0) {
            throw std::runtime_error(
                "Figmentum polygonize produced a degenerate vertex normal");
        }
        const double inverseLength = 1.0 / std::sqrt(lengthSquared);
        normal.x *= inverseLength;
        normal.y *= inverseLength;
        normal.z *= inverseLength;
    }
    return accumulated;
}

}  // namespace

// @implements spec/interface/figmentum-city-generation.md Geometry generation
// @implements spec/interface/figmentum-city-generation.md Error contract
PreparedFacilityGeometry prepareFacilityGeometry(
    const city::ManifestFacility& facility) {
    require(facility.id.isValid(), "facility game id is invalid");
    require(facility.figmentumKey.isValid(),
            "facility Figmentum key is invalid");
    const fg::BuildingParams recipe = convert(facility.recipe);
    const fg::Aabb bounds = fg::buildingBounds(recipe);
    const sim::Bounds3 convertedBounds = toBounds3(bounds);
    require(convertedBounds.min.x == facility.boundsMeters.min.x,
            "manifest recipe does not reproduce bounds.min.x");
    require(convertedBounds.min.y == facility.boundsMeters.min.y,
            "manifest recipe does not reproduce bounds.min.y");
    require(convertedBounds.min.z == facility.boundsMeters.min.z,
            "manifest recipe does not reproduce bounds.min.z");
    require(convertedBounds.max.x == facility.boundsMeters.max.x,
            "manifest recipe does not reproduce bounds.max.x");
    require(convertedBounds.max.y == facility.boundsMeters.max.y,
            "manifest recipe does not reproduce bounds.max.y");
    require(convertedBounds.max.z == facility.boundsMeters.max.z,
            "manifest recipe does not reproduce bounds.max.z");

    return {
        .figmentumKey = facility.figmentumKey,
        .cacheKey =
            {
                .schemaVersion = city::kFacilityGeometryCacheSchemaVersion,
                .generatorRevision = std::string(city::kFigmentumRevision),
                .recipeHash =
                    city::buildingRecipeFingerprint(facility.recipe),
                .polygonizeResolution =
                    city::kFirstPlayableFacilityPolygonizeResolution,
                .vertexFormatVersion = city::kFacilityVertexFormatVersion,
            },
        .recipe = recipe,
        .bounds = bounds,
    };
}

// @implements spec/interface/figmentum-city-generation.md Geometry generation
// @implements spec/interface/figmentum-city-generation.md Error contract
city::FacilityGeometry generateFacilityGeometry(
    const PreparedFacilityGeometry& prepared) {
    const fg::SdfModel model = fg::generateBuilding(prepared.recipe);
    if (model.terms.empty()) {
        throw std::runtime_error("Figmentum generated an empty facility SDF");
    }
    const fg::Mesh mesh = fg::polygonize(
        [&model](const fg::Vec3 point) { return model.eval(point); },
        prepared.bounds.min,
        prepared.bounds.max,
        city::kFirstPlayableFacilityPolygonizeResolution);
    if (mesh.vertices.empty() || mesh.indices.empty() ||
        mesh.indices.size() % 3 != 0) {
        throw std::runtime_error("Figmentum polygonize returned empty geometry");
    }

    city::FacilityGeometry result;
    result.figmentumKey = prepared.figmentumKey;
    result.cacheKey = prepared.cacheKey;
    result.positionsMeters.reserve(mesh.vertices.size());
    for (const fg::Vec3 vertex : mesh.vertices) {
        const sim::Vec3 converted = toVec3(vertex);
        if (!sim::isFinite(converted)) {
            throw std::runtime_error(
                "Figmentum polygonize returned a non-finite vertex");
        }
        result.positionsMeters.push_back(converted);
    }
    result.indices = mesh.indices;
    result.normals = generateNormals(result.positionsMeters, result.indices);
    return result;
}

}  // namespace konbini::adapters::figmentum
