#include "konbini/city/city_manifest_canonical.h"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <utility>

#include "canonical_bytes.h"

// @implements spec/interface/figmentum-city-generation.md Output: `CityManifest`

namespace konbini::city {

namespace {

void writePosition(CanonicalBytes& writer, const sim::Vec3 position) {
    writer.f64(position.x);
    writer.f64(position.y);
    writer.f64(position.z);
}

void writeBounds(CanonicalBytes& writer, const sim::Bounds3& bounds) {
    writePosition(writer, bounds.min);
    writePosition(writer, bounds.max);
}

void writeRecipe(CanonicalBytes& writer, const BuildingRecipe& recipe) {
    writer.text(recipe.kind);
    writer.text(recipe.roof);
    writePosition(writer, recipe.originMeters);
    writer.f64(recipe.footprintHalfXMeters);
    writer.f64(recipe.footprintHalfZMeters);
    writer.f64(recipe.heightMeters);
    writer.u8(recipe.hasRooftopProps ? 1U : 0U);
    writer.f64(recipe.blendMeters);
    writer.f64(recipe.roofBlendMeters);
    writer.i32(recipe.roofSteps);
    writer.u32(recipe.seed);
}

}  // namespace

// game 所有の `FacilityId` は意図的に書き出さない。gameplay 側の ID 採番が
// 変わっても、同じ都市 plan なら同じ canonical bytes になる必要がある。
// @implements spec/interface/figmentum-city-generation.md Output: `CityManifest`
std::vector<std::byte> serializeCityManifestCanonical(
    const CityManifest& manifest) {
    CanonicalBytes writer;
    writer.text("KonbiniCityManifest");
    writer.u32(manifest.canonicalVersion);
    writer.u32(manifest.schemaVersion);
    writer.u32(manifest.generatorSchemaVersion);
    writer.u32(manifest.generatorRecipeVersion);
    writer.text(manifest.generatorRevision);
    writer.u64(manifest.seed);
    writePosition(writer, manifest.stationAnchorMeters);
    writeBounds(writer, manifest.boundsMeters);

    std::vector<const ManifestFacility*> facilities;
    facilities.reserve(manifest.facilities.size());
    for (const ManifestFacility& facility : manifest.facilities) {
        facilities.push_back(&facility);
    }
    std::sort(facilities.begin(),
              facilities.end(),
              [](const ManifestFacility* left,
                 const ManifestFacility* right) {
                  return left->figmentumKey < right->figmentumKey;
              });
    // std::sort is unstable, so duplicate keys would make the canonical byte
    // stream depend on the caller's facility ordering. A canonical form has to
    // be total: reject the ambiguity instead of hashing an arbitrary order.
    for (std::size_t index = 1; index < facilities.size(); ++index) {
        if (facilities[index]->figmentumKey ==
            facilities[index - 1]->figmentumKey) {
            throw std::invalid_argument(
                "canonical city serialization requires unique Figmentum keys");
        }
    }
    writer.u64(facilities.size());
    for (const ManifestFacility* facility : facilities) {
        writer.u64(facility->figmentumKey.value());
        writer.i32(facility->cell.x);
        writer.i32(facility->cell.z);
        writer.u8(facility->cell.lotSlot);
        writer.u8(static_cast<std::uint8_t>(facility->role));
        writeRecipe(writer, facility->recipe);
        writeBounds(writer, facility->boundsMeters);
        writer.u8(facility->isBuildable ? 1U : 0U);
    }
    return std::move(writer).finish();
}

// `manifest.canonicalHash` 自身は byte 列へ含めないので、hash を格納した
// manifest を再度 hash しても同じ値になる。
// @implements spec/interface/figmentum-city-generation.md Error contract
std::uint64_t cityManifestCanonicalHash(const CityManifest& manifest) {
    return fnv1a64(serializeCityManifestCanonical(manifest));
}

}  // namespace konbini::city
