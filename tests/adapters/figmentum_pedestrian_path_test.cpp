#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "figmentum/gen/city_plan.h"
#include "figmentum/gen/pedestrian_path.h"

#include "../../src/adapters/figmentum/figmentum_pedestrian_projection.h"
#include "konbini/adapters/figmentum/figmentum_city_adapter.h"
#include "konbini/city/city_manifest.h"
#include "konbini/city/city_manifest_canonical.h"
#include "konbini/city/city_manifest_projection.h"
#include "konbini/city/grid_town.h"
#include "konbini/sim/entity_id.h"
#include "konbini/sim/pedestrian_route.h"

#include "../check.h"

// @implements spec/test/verification-strategy.md 原則
// @implements spec/interface/figmentum-city-generation.md Pedestrian path contract

namespace {

namespace city = konbini::city;
namespace sim = konbini::sim;
using konbini::adapters::figmentum::FigmentumCityAdapter;
using konbini::adapters::figmentum::convertPedestrianNetwork;
using konbini::adapters::figmentum::projectPedestrianNetwork;
using konbini::test::throwsException;

[[nodiscard]] city::CityManifest planManifest() {
    sim::GenerationalIdPool<sim::FacilityId> ids;
    return FigmentumCityAdapter{}.planFirstPlayableCity(ids);
}

[[nodiscard]] city::CityManifest rehashed(city::CityManifest manifest) {
    manifest.canonicalHash = city::cityManifestCanonicalHash(manifest);
    return manifest;
}

void testManifestCarriesThePinnedPathContract() {
    const city::CityManifest manifest = planManifest();
    CHECK(manifest.generatorRevision == city::kFigmentumRevision);
    CHECK(manifest.generatorRevision == "ff09a65db1a1db6711537a7ce49f207ca068c8b5");
    CHECK(manifest.canonicalVersion == 2);
    CHECK(manifest.pedestrianPaths.has_value());
    if (!manifest.pedestrianPaths.has_value()) {
        return;
    }
    const city::PedestrianPathContract& paths = *manifest.pedestrianPaths;
    CHECK(paths.schemaVersion == fg::kPedestrianNetworkSchemaVersion);
    CHECK(paths.recipeVersion == fg::kPedestrianNetworkRecipeVersion);
    // Default 5 x 5 blocks: 6 x 6 intersections, 2 * 6 * 5 street segments.
    const fg::CityPlanParams params{};
    const auto lines = static_cast<std::size_t>((params.blocksX + 1) * (params.blocksZ + 1));
    CHECK(paths.nodes.size() == lines);
    CHECK(paths.edges.size() ==
          static_cast<std::size_t>((params.blocksX * (params.blocksZ + 1)) +
                                   ((params.blocksX + 1) * params.blocksZ)));
    CHECK(paths.entrances.size() == manifest.facilities.size());
    CHECK_NO_THROW(city::validateCityManifest(manifest));
}

void testProjectionIsDeterministic() {
    const city::CityManifest first = planManifest();
    const city::CityManifest second = planManifest();
    CHECK(first.canonicalHash == second.canonicalHash);
    CHECK(city::serializeCityManifestCanonical(first) ==
          city::serializeCityManifestCanonical(second));
}

// The contract is part of the canonical bytes: a different path network is
// a different city.
void testContractIsPartOfCanonicalHash() {
    const city::CityManifest manifest = planManifest();
    city::CityManifest moved = manifest;
    moved.pedestrianPaths->nodes.front().positionMeters.x += 1.0;
    CHECK(city::cityManifestCanonicalHash(moved) != manifest.canonicalHash);
    CHECK(throwsException<std::invalid_argument>(
        [&] { city::validateCityManifest(moved); }));
}

void testValidationRejectsBrokenContracts() {
    const city::CityManifest manifest = planManifest();

    city::CityManifest missing = manifest;
    missing.pedestrianPaths.reset();
    CHECK(throwsException<std::invalid_argument>(
        [&] { city::validateCityManifest(rehashed(missing)); }));

    city::CityManifest version = manifest;
    version.pedestrianPaths->schemaVersion = 2;
    CHECK(throwsException<std::invalid_argument>(
        [&] { city::validateCityManifest(rehashed(version)); }));

    city::CityManifest droppedEntrance = manifest;
    droppedEntrance.pedestrianPaths->entrances.pop_back();
    CHECK(throwsException<std::invalid_argument>(
        [&] { city::validateCityManifest(rehashed(droppedEntrance)); }));

    city::CityManifest degenerate = manifest;
    degenerate.pedestrianPaths->edges.front().lengthMeters = 0.0;
    CHECK(throwsException<std::invalid_argument>(
        [&] { city::validateCityManifest(rehashed(degenerate)); }));

    city::CityManifest unordered = manifest;
    std::swap(unordered.pedestrianPaths->nodes[0], unordered.pedestrianPaths->nodes[1]);
    CHECK(throwsException<std::invalid_argument>(
        [&] { city::validateCityManifest(rehashed(unordered)); }));
}

void testAdapterRejectsForeignNetworks() {
    const fg::CityPlanParams params{};
    const fg::CityPlan plan = fg::planCity(params, city::kFirstPlayableWorldSeed);
    const fg::PedestrianNetwork network = fg::planPedestrianNetwork(params, plan);

    fg::PedestrianNetwork otherSeed = network;
    otherSeed.seed += 1;
    CHECK(throwsException<std::runtime_error>(
        [&] { return convertPedestrianNetwork(otherSeed, plan); }));

    fg::PedestrianNetwork newerRecipe = network;
    newerRecipe.recipeVersion += 1;
    CHECK(throwsException<std::runtime_error>(
        [&] { return convertPedestrianNetwork(newerRecipe, plan); }));

    // A Figmentum error surfaces as a load failure, not as an empty network.
    // PlanMismatch: the plan was generated around a different station anchor.
    fg::CityPlanParams mismatched = params;
    mismatched.stationAnchor.x += 1.0f;
    CHECK(throwsException<std::runtime_error>(
        [&] { return projectPedestrianNetwork(mismatched, plan); }));

    // InvalidParams: the lattice needs an odd block count per axis.
    fg::CityPlanParams invalid = params;
    invalid.blocksX += 1;
    CHECK(throwsException<std::runtime_error>(
        [&] { return projectPedestrianNetwork(invalid, plan); }));
}

// Every facility can walk to every buildable facility on the generated city,
// and the route starts and ends at the facilities' own entrances.
void testGeneratedCityRoutesBetweenFacilities() {
    const city::CityManifest manifest = planManifest();
    const std::optional<sim::PedestrianPathTable> table =
        city::projectPedestrianPathTable(manifest);
    CHECK(table.has_value());
    if (!table.has_value()) {
        return;
    }
    const auto& entrances = manifest.pedestrianPaths->entrances;
    for (const city::ManifestFacility& home : manifest.facilities) {
        for (const city::ManifestFacility& store : manifest.facilities) {
            if (!store.isBuildable) {
                continue;
            }
            const sim::PedestrianRoute route =
                sim::selectPedestrianRoute(*table, home.figmentumKey, store.figmentumKey);
            CHECK(route.status == sim::PedestrianRouteStatus::Routed);
            if (route.status != sim::PedestrianRouteStatus::Routed) {
                continue;
            }
            const auto findEntrance = [&](const sim::FigmentumFacilityKey key) {
                for (const city::PedestrianPathEntrance& entrance : entrances) {
                    if (entrance.facility == key) {
                        return entrance.positionMeters;
                    }
                }
                return sim::Vec3{};
            };
            const sim::Vec3 start = findEntrance(home.figmentumKey);
            const sim::Vec3 end = findEntrance(store.figmentumKey);
            CHECK(route.waypointsMeters.front().x == start.x);
            CHECK(route.waypointsMeters.front().z == start.z);
            CHECK(route.waypointsMeters.back().x == end.x);
            CHECK(route.waypointsMeters.back().z == end.z);
        }
    }
}

void testGridTownHasNoPedestrianNetwork() {
    sim::GenerationalIdPool<sim::FacilityId> ids;
    const city::GeneratedCity grid = city::makeGridTown(ids);
    CHECK(!grid.manifest.pedestrianPaths.has_value());
    CHECK(!city::projectPedestrianPathTable(grid.manifest).has_value());
}

}  // namespace

int main() {
    testManifestCarriesThePinnedPathContract();
    testProjectionIsDeterministic();
    testContractIsPartOfCanonicalHash();
    testValidationRejectsBrokenContracts();
    testAdapterRejectsForeignNetworks();
    testGeneratedCityRoutesBetweenFacilities();
    testGridTownHasNoPedestrianNetwork();
    return konbini::test::summarize("konbini_figmentum_pedestrian_path_tests");
}
