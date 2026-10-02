#include <memory>
#include <stdexcept>
#include <utility>

#include "konbini/city/derived_geometry_cache_policy.h"
#include "konbini/city/facility_geometry.h"
#include "konbini/city/facility_geometry_cache.h"

#include "../check.h"

// @implements spec/test/verification-strategy.md 1. Data / ID unit tests
// @implements spec/interface/mobile-platform.md Assets and generated geometry

namespace {

using konbini::city::DerivedGeometryCachePolicy;
using konbini::city::FacilityGeometry;
using konbini::city::FacilityGeometryCache;
using konbini::city::FacilityGeometryCacheKey;
using konbini::city::GeometryEvictionScope;
using konbini::city::GeometryOrigin;

[[nodiscard]] FacilityGeometryCacheKey baseKey() {
    return {
        .generatorRevision = "3ee998f487d984f54003c4ec3c4f7ba00b53eec3",
        .recipeHash = 0x1234,
        .polygonizeResolution = konbini::city::kFirstPlayableFacilityPolygonizeResolution,
    };
}

[[nodiscard]] std::shared_ptr<const FacilityGeometry> geometryWith(FacilityGeometryCacheKey key) {
    auto geometry = std::make_shared<FacilityGeometry>();
    geometry->cacheKey = std::move(key);
    return geometry;
}

void keySeparatesEveryGeometryInput() {
    const FacilityGeometryCacheKey base = baseKey();
    CHECK(base.schemaVersion == konbini::city::kFacilityGeometryCacheSchemaVersion);
    CHECK(base.lod == konbini::city::kFirstPlayableFacilityLod);
    CHECK(base.vertexFormatVersion == konbini::city::kFacilityVertexFormatVersion);

    FacilityGeometryCacheKey revision = base;
    revision.generatorRevision = "0000000000000000000000000000000000000000";
    FacilityGeometryCacheKey schema = base;
    schema.schemaVersion += 1;
    FacilityGeometryCacheKey recipe = base;
    recipe.recipeHash += 1;
    FacilityGeometryCacheKey lod = base;
    lod.lod += 1;
    FacilityGeometryCacheKey resolution = base;
    resolution.polygonizeResolution += 1;
    FacilityGeometryCacheKey vertexFormat = base;
    vertexFormat.vertexFormatVersion += 1;
    for (const FacilityGeometryCacheKey& changed :
         {revision, schema, recipe, lod, resolution, vertexFormat}) {
        CHECK(changed != base);
    }

    // One geometry per LOD of the same recipe.
    FacilityGeometryCache cache;
    static_cast<void>(cache.insert(geometryWith(base)));
    static_cast<void>(cache.insert(geometryWith(lod)));
    CHECK(cache.size() == 2);
    CHECK(cache.find(lod) != cache.find(base));
    CHECK_THROWS(std::invalid_argument, static_cast<void>(cache.insert(geometryWith(vertexFormat))));
}

// The map key orders through an explicit member-wise `<` (no defaulted `<=>`,
// which Apple libc++ deletes for std::string members). It must stay a strict
// weak order that separates every input, compared in declaration order.
void keyOrderIsStrictWeakInDeclarationOrder() {
    const FacilityGeometryCacheKey base = baseKey();
    CHECK(!(base < base));

    FacilityGeometryCacheKey revision = base;
    revision.generatorRevision = "ffffffffffffffffffffffffffffffffffffffff";
    FacilityGeometryCacheKey recipe = base;
    recipe.recipeHash += 1;
    FacilityGeometryCacheKey resolution = base;
    resolution.polygonizeResolution += 1;
    FacilityGeometryCacheKey lod = base;
    lod.lod += 1;
    FacilityGeometryCacheKey vertexFormat = base;
    vertexFormat.vertexFormatVersion += 1;
    for (const FacilityGeometryCacheKey& larger :
         {revision, recipe, resolution, lod, vertexFormat}) {
        CHECK(base < larger);
        CHECK(!(larger < base));
    }

    // Earlier members dominate later ones.
    FacilityGeometryCacheKey olderSchema = revision;
    olderSchema.schemaVersion -= 1;
    CHECK(olderSchema < base);
    FacilityGeometryCacheKey smallerRecipe = vertexFormat;
    smallerRecipe.recipeHash -= 1;
    CHECK(smallerRecipe < base);
}

void policyNeverEvictsPackagedGeometry() {
    const DerivedGeometryCachePolicy policy;
    for (const GeometryEvictionScope scope :
         {GeometryEvictionScope::None, GeometryEvictionScope::Unused,
          GeometryEvictionScope::AllRegenerable}) {
        CHECK(!policy.shouldEvict(GeometryOrigin::PackagedLowLod, false, scope));
        CHECK(!policy.shouldEvict(GeometryOrigin::PackagedLowLod, true, scope));
    }
    CHECK(!policy.shouldEvict(GeometryOrigin::Regenerated, false, GeometryEvictionScope::None));
    CHECK(policy.shouldEvict(GeometryOrigin::Regenerated, false, GeometryEvictionScope::Unused));
    CHECK(!policy.shouldEvict(GeometryOrigin::Regenerated, true, GeometryEvictionScope::Unused));
    CHECK(policy.shouldEvict(GeometryOrigin::Regenerated, true,
                             GeometryEvictionScope::AllRegenerable));
}

void cacheEvictsInStages() {
    FacilityGeometryCache cache;
    const DerivedGeometryCachePolicy policy;

    FacilityGeometryCacheKey packagedKey = baseKey();
    packagedKey.lod = 2;
    FacilityGeometryCacheKey drawnKey = baseKey();
    drawnKey.recipeHash = 0x5678;
    FacilityGeometryCacheKey idleKey = baseKey();
    idleKey.recipeHash = 0x9abc;

    static_cast<void>(cache.insert(geometryWith(packagedKey), GeometryOrigin::PackagedLowLod));
    const auto drawn = cache.insert(geometryWith(drawnKey));
    static_cast<void>(cache.insert(geometryWith(idleKey)));
    CHECK(cache.size() == 3);

    CHECK(cache.evict(policy, GeometryEvictionScope::None) == 0);
    CHECK(cache.evict(policy, GeometryEvictionScope::Unused) == 1);
    CHECK(cache.find(idleKey) == nullptr);
    CHECK(cache.find(drawnKey) == drawn);

    CHECK(cache.evict(policy, GeometryEvictionScope::AllRegenerable) == 1);
    CHECK(cache.find(drawnKey) == nullptr);
    // The holder keeps its instance; only the cache reference went.
    CHECK(drawn->cacheKey == drawnKey);
    CHECK(cache.find(packagedKey) != nullptr);
    CHECK(cache.size() == 1);

    // A dropped key regenerates on the next miss.
    static_cast<void>(cache.insert(geometryWith(idleKey)));
    CHECK(cache.find(idleKey) != nullptr);
}

}  // namespace

int main() {
    keySeparatesEveryGeometryInput();
    keyOrderIsStrictWeakInDeclarationOrder();
    policyNeverEvictsPackagedGeometry();
    cacheEvictsInStages();
    return konbini::test::summarize("konbini_derived_geometry_cache_tests");
}
