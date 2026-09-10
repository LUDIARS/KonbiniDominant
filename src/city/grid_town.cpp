// @implements spec/feature/grid-town-and-vector-ui.md Grid town
#include "konbini/city/grid_town.h"
#include <utility>
#include "konbini/city/city_manifest_canonical.h"

// @implements spec/plan/problem_logs/2026-09-10-phase1-placement-and-ground.md
namespace konbini::city {
GeneratedCity makeGridTown(sim::GenerationalIdPool<sim::FacilityId>& ids) {
    auto staged = ids;
    GeneratedCity city;
    auto& manifest = city.manifest;
    manifest.generatorSchemaVersion = 1;
    manifest.generatorRecipeVersion = 1;
    manifest.generatorRevision = kGridTownRevision;
    manifest.seed = kFirstPlayableWorldSeed;
    const double half = kGridTownSide * kGridCellMeters * 0.5;
    manifest.boundsMeters = {{-half, 0, -half}, {half, 0.2, half}};
    for (int z = 0; z < kGridTownSide; ++z) {
        for (int x = 0; x < kGridTownSide; ++x) {
            const auto key = static_cast<std::uint32_t>(z * kGridTownSide + x + 1);
            const double px = -half + (x + 0.5) * kGridCellMeters;
            const double pz = -half + (z + 0.5) * kGridCellMeters;
            const double radius = kGridCellMeters * 0.5;
            manifest.facilities.push_back({
                .id = staged.acquire(), .figmentumKey = {.rawValue = key},
                .cell = {.x = x, .z = z}, .role = FacilityRole::Residential,
                .recipe = {.kind = "grid-cell", .roof = "ground",
                    .originMeters = {px, 0, pz}, .footprintHalfXMeters = radius,
                    .footprintHalfZMeters = radius, .heightMeters = 0.2,
                    .roofSteps = 1, .seed = key},
                .boundsMeters = {{px-radius, 0, pz-radius}, {px+radius, 0.2, pz+radius}},
                .isBuildable = true});
        }
    }
    manifest.canonicalHash = cityManifestCanonicalHash(manifest);
    validateCityManifest(manifest);
    ids = std::move(staged);
    // The game-owned ground mesh is built from these cells; no building mesh
    // or Figmentum polygonization is needed for a flat town.
    return city;
}
}
