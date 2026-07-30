#include "konbini/city/building_recipe_fingerprint.h"

#include "canonical_bytes.h"

namespace konbini::city {

// @implements spec/interface/figmentum-city-generation.md Geometry generation
std::uint64_t buildingRecipeFingerprint(const BuildingRecipe& recipe) {
    CanonicalBytes writer;
    writer.text("KonbiniBuildingRecipe");
    writer.u32(kBuildingRecipeFingerprintVersion);
    writer.text(recipe.kind);
    writer.text(recipe.roof);
    writer.f64(recipe.originMeters.x);
    writer.f64(recipe.originMeters.y);
    writer.f64(recipe.originMeters.z);
    writer.f64(recipe.footprintHalfXMeters);
    writer.f64(recipe.footprintHalfZMeters);
    writer.f64(recipe.heightMeters);
    writer.u8(recipe.hasRooftopProps ? 1U : 0U);
    writer.f64(recipe.blendMeters);
    writer.f64(recipe.roofBlendMeters);
    writer.i32(recipe.roofSteps);
    writer.u32(recipe.seed);
    return fnv1a64(writer.bytes());
}

}  // namespace konbini::city
