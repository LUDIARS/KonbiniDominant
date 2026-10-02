#include <cstddef>
#include <memory>
#include <stdexcept>

#include "konbini/adapters/figmentum/figmentum_city_adapter.h"
#include "konbini/app/platform/mobile_graphics_profile.h"
#include "konbini/city/derived_geometry_cache_policy.h"
#include "konbini/city/facility_geometry.h"
#include "konbini/sim/entity_id.h"

#include "../check.h"

// @implements spec/test/verification-strategy.md 原則
// @implements spec/interface/mobile-platform.md Assets and generated geometry

namespace {

namespace city = konbini::city;
namespace sim = konbini::sim;
using konbini::adapters::figmentum::FigmentumCityAdapter;
using konbini::app::mobileGraphicsProfile;
using konbini::app::MobileGraphicsProfileId;

[[nodiscard]] city::GeneratedCity generate(const FigmentumCityAdapter& adapter) {
    sim::GenerationalIdPool<sim::FacilityId> ids;
    return adapter.generateFirstPlayableCity(ids);
}

[[nodiscard]] std::size_t totalVertices(const city::GeneratedCity& generated) {
    std::size_t vertices = 0;
    for (const auto& geometry : generated.geometry) {
        vertices += geometry->positionsMeters.size();
    }
    return vertices;
}

void testDefaultAdapterKeepsTheDesktopLevel() {
    const FigmentumCityAdapter adapter;
    CHECK(adapter.meshDetail() == city::kFirstPlayableFacilityMeshDetail);
    const city::GeneratedCity generated = generate(adapter);
    CHECK(!generated.geometry.empty());
    for (const auto& geometry : generated.geometry) {
        CHECK(geometry->cacheKey.polygonizeResolution ==
              city::kFirstPlayableFacilityPolygonizeResolution);
        CHECK(geometry->cacheKey.lod == city::kFirstPlayableFacilityLod);
    }
}

// The same planCity() manifest, polygonized at the mobile profile's level:
// the city is identical, only the derived mesh is coarser.
void testMobileProfileMeshesTheSameCityCoarser() {
    const auto& low = mobileGraphicsProfile(MobileGraphicsProfileId::MobileLow);
    const FigmentumCityAdapter desktop;
    const FigmentumCityAdapter mobile(low.facilityMesh);
    const city::GeneratedCity desktopCity = generate(desktop);
    const city::GeneratedCity mobileCity = generate(mobile);

    CHECK(mobileCity.manifest.canonicalHash == desktopCity.manifest.canonicalHash);
    CHECK(mobileCity.geometry.size() == desktopCity.geometry.size());
    for (const auto& geometry : mobileCity.geometry) {
        CHECK(geometry->cacheKey.polygonizeResolution == low.facilityMesh.polygonizeResolution);
        CHECK(geometry->cacheKey.lod == low.facilityMesh.lod);
    }
    CHECK(totalVertices(mobileCity) < totalVertices(desktopCity));
}

void testInvalidResolutionIsRejected() {
    const FigmentumCityAdapter adapter(city::FacilityMeshDetail{.polygonizeResolution = 0, .lod = 1});
    sim::GenerationalIdPool<sim::FacilityId> ids;
    CHECK_THROWS(std::invalid_argument, static_cast<void>(adapter.generateFirstPlayableCity(ids)));
}

// Memory pressure drops regenerable CPU meshes in stages and a dropped recipe
// regenerates on the next build, outside any frame.
void testDerivedGeometryEvictsInStages() {
    const FigmentumCityAdapter adapter(
        mobileGraphicsProfile(MobileGraphicsProfileId::MobileHigh).facilityMesh);
    {
        const city::GeneratedCity held = generate(adapter);
        CHECK(adapter.evictDerivedGeometry(city::GeometryEvictionScope::None) == 0);
    }
    // The previous city is gone: moderate pressure may drop all of it.
    const std::size_t unused = adapter.evictDerivedGeometry(city::GeometryEvictionScope::Unused);
    CHECK(unused > 0);

    const city::GeneratedCity regenerated = generate(adapter);
    CHECK(!regenerated.geometry.empty());
    CHECK(adapter.evictDerivedGeometry(city::GeometryEvictionScope::AllRegenerable) == unused);
    // Holders keep their shared instance alive after a critical eviction.
    CHECK(regenerated.geometry.front()->positionsMeters.size() > 0);
}

}  // namespace

int main() {
    testDefaultAdapterKeepsTheDesktopLevel();
    testMobileProfileMeshesTheSameCityCoarser();
    testInvalidResolutionIsRejected();
    testDerivedGeometryEvictsInStages();
    return konbini::test::summarize("konbini_figmentum_mesh_detail_tests");
}
