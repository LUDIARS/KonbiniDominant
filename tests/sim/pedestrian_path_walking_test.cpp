#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "konbini/sim/canonical_snapshot.h"
#include "konbini/sim/first_playable_simulation.h"
#include "konbini/sim/pedestrian_path_table.h"
#include "konbini/sim/pedestrian_route.h"
#include "konbini/sim/resident_presentation.h"

#include "../check.h"

// @implements spec/test/verification-strategy.md 2. Deterministic simulation
// @implements spec/feature/npc-conversations-and-placement-feedback.md Pedestrian path walking

namespace {

using konbini::sim::ChainId;
using konbini::sim::CommandSourcePriority;
using konbini::sim::EntityId;
using konbini::sim::FacilityId;
using konbini::sim::FacilityTable;
using konbini::sim::FigmentumFacilityKey;
using konbini::sim::FirstPlayableContent;
using konbini::sim::FirstPlayableSimulation;
using konbini::sim::PedestrianEdgeKey;
using konbini::sim::PedestrianEdgeRow;
using konbini::sim::PedestrianEntranceRow;
using konbini::sim::PedestrianNodeRow;
using konbini::sim::PedestrianPathTable;
using konbini::sim::PedestrianRoute;
using konbini::sim::PedestrianRouteStatus;
using konbini::sim::PopulationCellId;
using konbini::sim::PopulationCellRow;
using konbini::sim::PopulationCellTable;
using konbini::sim::ResidentPathContext;
using konbini::sim::ResidentPresentation;
using konbini::sim::ResidentPresentationContent;
using konbini::sim::ResidentRouteState;
using konbini::sim::ResidentTripPhase;
using konbini::sim::SelectChainCommand;
using konbini::sim::StoreId;
using konbini::sim::StoreRow;
using konbini::sim::StoreTable;
using konbini::sim::Vec3;
using konbini::sim::WorldEntityIds;
using konbini::sim::projectResidentPresentations;
using konbini::sim::selectPedestrianRoute;
using konbini::test::approxEqual;
using konbini::test::throwsException;

constexpr FigmentumFacilityKey kHomeKey{.rawValue = 10};
constexpr FigmentumFacilityKey kStoreKey{.rawValue = 20};
constexpr FigmentumFacilityKey kIslandKey{.rawValue = 30};

// Unit square A(0,0) B(10,0) C(0,10) D(10,10) plus an isolated node E.
// A -> D has two equal 20 m routes: A-B-D and A-C-D.
constexpr std::uint64_t kNodeA = 100;
constexpr std::uint64_t kNodeB = 101;
constexpr std::uint64_t kNodeC = 102;
constexpr std::uint64_t kNodeD = 103;
constexpr std::uint64_t kNodeE = 104;

[[nodiscard]] std::vector<PedestrianNodeRow> squareNodes() {
    return {
        {.key = kNodeA, .positionMeters = {0.0, 0.0, 0.0}},
        {.key = kNodeB, .positionMeters = {10.0, 0.0, 0.0}},
        {.key = kNodeC, .positionMeters = {0.0, 0.0, 10.0}},
        {.key = kNodeD, .positionMeters = {10.0, 0.0, 10.0}},
        {.key = kNodeE, .positionMeters = {50.0, 0.0, 50.0}},
    };
}

struct SquareEdgeKeys {
    PedestrianEdgeKey ab = 0;
    PedestrianEdgeKey ac = 0;
    PedestrianEdgeKey bd = 0;
    PedestrianEdgeKey cd = 0;
};

// Rows must be in key order, so they are sorted by the caller-chosen keys.
[[nodiscard]] std::vector<PedestrianEdgeRow> squareEdges(const SquareEdgeKeys keys) {
    std::vector<PedestrianEdgeRow> edges{
        {.key = keys.ab, .from = kNodeA, .to = kNodeB, .lengthMeters = 10.0},
        {.key = keys.ac, .from = kNodeA, .to = kNodeC, .lengthMeters = 10.0},
        {.key = keys.bd, .from = kNodeB, .to = kNodeD, .lengthMeters = 10.0},
        {.key = keys.cd, .from = kNodeC, .to = kNodeD, .lengthMeters = 10.0},
    };
    std::sort(edges.begin(), edges.end(), [](const auto& left, const auto& right) {
        return left.key < right.key;
    });
    return edges;
}

[[nodiscard]] std::vector<PedestrianEntranceRow> squareEntrances() {
    return {
        {.facility = kHomeKey, .positionMeters = {0.0, 0.0, -2.0}, .node = kNodeA},
        {.facility = kStoreKey, .positionMeters = {12.0, 0.0, 10.0}, .node = kNodeD},
        {.facility = kIslandKey, .positionMeters = {50.0, 0.0, 52.0}, .node = kNodeE},
    };
}

[[nodiscard]] PedestrianPathTable squareTable(const SquareEdgeKeys keys) {
    const auto nodes = squareNodes();
    const auto edges = squareEdges(keys);
    const auto entrances = squareEntrances();
    return PedestrianPathTable::build(nodes, edges, entrances);
}

// Keys where the B-side route owns the smallest edge key.
constexpr SquareEdgeKeys kBFirstKeys{.ab = 1, .ac = 2, .bd = 3, .cd = 4};
// Same geometry with the C side owning the smallest keys.
constexpr SquareEdgeKeys kCFirstKeys{.ab = 3, .ac = 1, .bd = 4, .cd = 2};

[[nodiscard]] bool sameXZ(const Vec3 left, const Vec3 right) {
    return left.x == right.x && left.z == right.z;
}

// --- table validation -------------------------------------------------

void testTableRejectsMalformedInput() {
    const auto nodes = squareNodes();
    const auto edges = squareEdges(kBFirstKeys);
    const auto entrances = squareEntrances();

    auto unsortedNodes = nodes;
    std::swap(unsortedNodes[0], unsortedNodes[1]);
    CHECK(throwsException<std::invalid_argument>(
        [&] { return PedestrianPathTable::build(unsortedNodes, edges, entrances); }));

    auto nonFinite = nodes;
    nonFinite[2].positionMeters.x = std::nan("");
    CHECK(throwsException<std::invalid_argument>(
        [&] { return PedestrianPathTable::build(nonFinite, edges, entrances); }));

    auto degenerate = edges;
    degenerate[0].lengthMeters = 0.0;
    CHECK(throwsException<std::invalid_argument>(
        [&] { return PedestrianPathTable::build(nodes, degenerate, entrances); }));

    auto unknown = edges;
    unknown[0].to = 999;
    CHECK(throwsException<std::invalid_argument>(
        [&] { return PedestrianPathTable::build(nodes, unknown, entrances); }));

    auto reversed = edges;
    std::swap(reversed[0].from, reversed[0].to);
    CHECK(throwsException<std::invalid_argument>(
        [&] { return PedestrianPathTable::build(nodes, reversed, entrances); }));

    auto duplicateEntrance = entrances;
    duplicateEntrance[1].facility = duplicateEntrance[0].facility;
    CHECK(throwsException<std::invalid_argument>(
        [&] { return PedestrianPathTable::build(nodes, edges, duplicateEntrance); }));

    auto danglingEntrance = entrances;
    danglingEntrance[0].node = 999;
    CHECK(throwsException<std::invalid_argument>(
        [&] { return PedestrianPathTable::build(nodes, edges, danglingEntrance); }));
}

// --- route selection --------------------------------------------------

void testEqualLengthTieBreakFollowsStableEdgeKey() {
    const PedestrianRoute viaB =
        selectPedestrianRoute(squareTable(kBFirstKeys), kHomeKey, kStoreKey);
    CHECK(viaB.status == PedestrianRouteStatus::Routed);
    CHECK((viaB.edges == std::vector<PedestrianEdgeKey>{kBFirstKeys.ab, kBFirstKeys.bd}));
    CHECK(viaB.waypointsMeters.size() == 5);
    if (viaB.waypointsMeters.size() == 5) {
        CHECK(sameXZ(viaB.waypointsMeters[0], {0.0, 0.0, -2.0}));
        CHECK(sameXZ(viaB.waypointsMeters[2], {10.0, 0.0, 0.0}));
        CHECK(sameXZ(viaB.waypointsMeters[4], {12.0, 0.0, 10.0}));
    }
    // 2 m entrance stub + 20 m of street + 2 m entrance stub.
    CHECK(viaB.lengthMeters == 24.0);

    // Only the keys changed: the choice must follow them, not row order or
    // geometry.
    const PedestrianRoute viaC =
        selectPedestrianRoute(squareTable(kCFirstKeys), kHomeKey, kStoreKey);
    CHECK(viaC.status == PedestrianRouteStatus::Routed);
    CHECK((viaC.edges == std::vector<PedestrianEdgeKey>{kCFirstKeys.ac, kCFirstKeys.cd}));
    CHECK(viaC.lengthMeters == viaB.lengthMeters);

    // Repeated selection is bit-identical.
    const PedestrianRoute again =
        selectPedestrianRoute(squareTable(kBFirstKeys), kHomeKey, kStoreKey);
    CHECK(again.edges == viaB.edges);
    CHECK(again.lengthMeters == viaB.lengthMeters);
}

void testShorterRouteBeatsSmallerKeys() {
    auto edges = squareEdges(kBFirstKeys);
    // Make the B side longer; the C side must win despite larger keys.
    for (PedestrianEdgeRow& edge : edges) {
        if (edge.key == kBFirstKeys.bd) {
            edge.lengthMeters = 10.5;
        }
    }
    const auto nodes = squareNodes();
    const auto entrances = squareEntrances();
    const PedestrianPathTable table =
        PedestrianPathTable::build(nodes, edges, entrances);
    const PedestrianRoute route = selectPedestrianRoute(table, kHomeKey, kStoreKey);
    CHECK(route.status == PedestrianRouteStatus::Routed);
    CHECK((route.edges == std::vector<PedestrianEdgeKey>{kBFirstKeys.ac, kBFirstKeys.cd}));
}

void testUnreachableAndMissingEntranceAreExplicit() {
    const PedestrianPathTable table = squareTable(kBFirstKeys);
    const PedestrianRoute island = selectPedestrianRoute(table, kHomeKey, kIslandKey);
    CHECK(island.status == PedestrianRouteStatus::Unreachable);
    CHECK(island.waypointsMeters.empty());
    CHECK(island.edges.empty());

    const PedestrianRoute missing =
        selectPedestrianRoute(table, kHomeKey, FigmentumFacilityKey{.rawValue = 99});
    CHECK(missing.status == PedestrianRouteStatus::MissingEntrance);
    CHECK(missing.waypointsMeters.empty());

    const PedestrianRoute same = selectPedestrianRoute(table, kStoreKey, kStoreKey);
    CHECK(same.status == PedestrianRouteStatus::Routed);
    CHECK(same.waypointsMeters.size() == 1);
    CHECK(same.edges.empty());
    CHECK(same.lengthMeters == 0.0);
}

// --- resident projection ----------------------------------------------

constexpr std::uint32_t kTicksPerSecond = 2;
constexpr std::uint64_t kHomeDwellTicks = 4;
constexpr std::uint64_t kStoreDwellTicks = 6;
// 24 m at 2 m/s and 2 ticks/s.
constexpr std::uint64_t kTravelTicks = 24;
constexpr std::uint64_t kStoreStartTick = kHomeDwellTicks + kTravelTicks;
constexpr std::uint64_t kReturnStartTick = kStoreStartTick + kStoreDwellTicks;
constexpr std::uint64_t kCycleTicks = kReturnStartTick + kTravelTicks;

const FacilityId kHomeFacility{EntityId{3, 1}};
const FacilityId kStoreFacility{EntityId{4, 1}};
const FacilityId kIslandFacility{EntityId{5, 1}};
const StoreId kStoreId{EntityId{2, 1}};
const StoreId kIslandStoreId{EntityId{6, 1}};
const PopulationCellId kCellId{EntityId{7, 1}};

[[nodiscard]] ResidentPresentationContent residentContent() {
    return {
        .samplesPerPopulationCell = 1,
        .walkingSpeedMetersPerSecond = 2.0,
        .homeDwellTicks = static_cast<std::uint32_t>(kHomeDwellTicks),
        .storeDwellTicks = static_cast<std::uint32_t>(kStoreDwellTicks),
        .speechDurationTicks = 2,
        .bubbleHeightMeters = 2.2,
        .bubbleMaxDistanceMeters = 50.0,
        .remarks = {{std::string("ALPHA"), std::string("BETA"), std::string("GAMMA")}},
    };
}

[[nodiscard]] FacilityTable residentFacilities() {
    FacilityTable facilities;
    const auto add = [&](const FacilityId id, const FigmentumFacilityKey key,
                         const Vec3 position) {
        facilities.append({
            .id = id,
            .figmentumKey = key,
            .positionMeters = position,
            .boundsMeters = {{position.x - 1.0, 0.0, position.z - 1.0},
                             {position.x + 1.0, 3.0, position.z + 1.0}},
            .isBuildable = true,
        });
    };
    add(kHomeFacility, kHomeKey, {0.0, 0.0, -4.0});
    add(kStoreFacility, kStoreKey, {14.0, 0.0, 10.0});
    add(kIslandFacility, kIslandKey, {50.0, 0.0, 54.0});
    return facilities;
}

[[nodiscard]] StoreTable residentStores() {
    StoreTable stores;
    stores.append({.id = kStoreId,
                   .facilityId = kStoreFacility,
                   .chain = ChainId::Losan,
                   .positionMeters = {14.0, 0.0, 10.0},
                   .zocRadiusMeters = 30.0});
    stores.append({.id = kIslandStoreId,
                   .facilityId = kIslandFacility,
                   .chain = ChainId::Losan,
                   .positionMeters = {50.0, 0.0, 54.0},
                   .zocRadiusMeters = 30.0});
    return stores;
}

[[nodiscard]] PopulationCellTable residentCells(const StoreId store) {
    PopulationCellTable cells;
    cells.append({.id = kCellId,
                  .facilityId = kHomeFacility,
                  .positionMeters = {0.0, 0.5, -4.0},
                  .population = 10,
                  .assignedStore = store,
                  .preferredChain = ChainId::Losan});
    return cells;
}

// Samples every tick of a full cycle and returns them in cycle order of the
// completed tick, so each phase is observed regardless of the phase offset.
[[nodiscard]] std::vector<ResidentPresentation> sampleCycle(
    const StoreId store, const ResidentPathContext* const paths) {
    const ResidentPresentationContent content = residentContent();
    const FacilityTable facilities = residentFacilities();
    const StoreTable stores = residentStores();
    const PopulationCellTable cells = residentCells(store);
    std::vector<ResidentPresentation> samples;
    for (std::uint64_t tick = 0; tick < kCycleTicks; ++tick) {
        const auto residents = projectResidentPresentations(
            tick, 42, kTicksPerSecond, content, cells, stores, paths);
        CHECK(residents.size() == 1);
        if (!residents.empty()) {
            samples.push_back(residents.front());
        }
    }
    return samples;
}

// The point lies on one of the route's axis-aligned street segments or
// entrance stubs, and the yaw points along that segment.
[[nodiscard]] bool onRouteSegmentFacingAlong(const ResidentPresentation& resident,
                                             const std::vector<Vec3>& route,
                                             const bool returning) {
    const Vec3 point = resident.positionMeters;
    for (std::size_t index = 1; index < route.size(); ++index) {
        const Vec3 start = returning ? route[index] : route[index - 1];
        const Vec3 end = returning ? route[index - 1] : route[index];
        const double minX = std::min(start.x, end.x);
        const double maxX = std::max(start.x, end.x);
        const double minZ = std::min(start.z, end.z);
        const double maxZ = std::max(start.z, end.z);
        const bool inside = point.x >= minX - 1e-9 && point.x <= maxX + 1e-9 &&
                            point.z >= minZ - 1e-9 && point.z <= maxZ + 1e-9;
        // Every segment here is axis-aligned, so the box test is exact.
        if (!inside) {
            continue;
        }
        const double yaw = std::atan2(end.x - start.x, end.z - start.z);
        if (approxEqual(resident.yawRadians, yaw, 1e-12)) {
            return true;
        }
    }
    return false;
}

void testResidentWalksAlongTheSelectedPath() {
    const FacilityTable facilities = residentFacilities();
    const PedestrianPathTable table = squareTable(kBFirstKeys);
    const ResidentPathContext paths{.facilities = facilities, .paths = table};
    const std::vector<Vec3> route{
        {0.0, 0.5, -2.0}, {0.0, 0.5, 0.0}, {10.0, 0.5, 0.0},
        {10.0, 0.5, 10.0}, {12.0, 0.5, 10.0}};

    std::size_t walkingOut = 0;
    std::size_t walkingBack = 0;
    bool sawCorner = false;
    for (const ResidentPresentation& resident : sampleCycle(kStoreId, &paths)) {
        CHECK(resident.route == ResidentRouteState::PedestrianPath);
        CHECK(resident.targetStore == kStoreId);
        CHECK(resident.positionMeters.y == 0.5);
        switch (resident.phase) {
            case ResidentTripPhase::AtHome:
                CHECK(sameXZ(resident.positionMeters, route.front()));
                // First leg leaves the home entrance toward +Z.
                CHECK(resident.yawRadians == 0.0);
                break;
            case ResidentTripPhase::WalkingToStore:
                ++walkingOut;
                CHECK(onRouteSegmentFacingAlong(resident, route, false));
                sawCorner = sawCorner || sameXZ(resident.positionMeters, route[2]);
                break;
            case ResidentTripPhase::AtStore:
                CHECK(sameXZ(resident.positionMeters, route.back()));
                // Arrives along the last stub, facing +X.
                CHECK(approxEqual(resident.yawRadians, std::numbers::pi / 2.0, 1e-12));
                break;
            case ResidentTripPhase::WalkingHome:
                ++walkingBack;
                CHECK(onRouteSegmentFacingAlong(resident, route, true));
                break;
        }
    }
    CHECK(walkingOut == kTravelTicks);
    CHECK(walkingBack == kTravelTicks);
    // 12 m into a 24 m route at 1 m/tick is exactly the B corner; never the
    // straight-line midpoint between home and store.
    CHECK(sawCorner);
}

void testUnreachableResidentStaysHomeWithoutFallback() {
    const FacilityTable facilities = residentFacilities();
    const PedestrianPathTable table = squareTable(kBFirstKeys);
    const ResidentPathContext paths{.facilities = facilities, .paths = table};
    for (const ResidentPresentation& resident : sampleCycle(kIslandStoreId, &paths)) {
        CHECK(resident.route == ResidentRouteState::Unreachable);
        CHECK(resident.phase == ResidentTripPhase::AtHome);
        CHECK(resident.targetStore == kIslandStoreId);
        CHECK(sameXZ(resident.positionMeters, {0.0, 0.5, -4.0}));
        CHECK(!resident.speech.has_value());
    }
}

void testMissingEntranceIsExplicit() {
    const FacilityTable facilities = residentFacilities();
    const auto nodes = squareNodes();
    const auto edges = squareEdges(kBFirstKeys);
    std::vector<PedestrianEntranceRow> entrances = squareEntrances();
    entrances.erase(entrances.begin() + 1);  // the store's entrance
    const PedestrianPathTable table =
        PedestrianPathTable::build(nodes, edges, entrances);
    const ResidentPathContext paths{.facilities = facilities, .paths = table};
    for (const ResidentPresentation& resident : sampleCycle(kStoreId, &paths)) {
        CHECK(resident.route == ResidentRouteState::MissingEntrance);
        CHECK(resident.phase == ResidentTripPhase::AtHome);
    }
}

void testWithoutNetworkKeepsTheStraightLineBaseline() {
    for (const ResidentPresentation& resident : sampleCycle(kStoreId, nullptr)) {
        CHECK(resident.route == ResidentRouteState::DirectLine);
    }
}

// --- gameplay isolation -------------------------------------------------

// 4 x 4 facilities on a 15 m pitch, with a 5 x 5 street lattice between them.
constexpr std::uint32_t kGrid = 4;
constexpr double kPitch = 15.0;

[[nodiscard]] FacilityTable gridFacilities(WorldEntityIds& ids) {
    FacilityTable facilities;
    for (std::uint32_t i = 0; i < kGrid * kGrid; ++i) {
        const auto id = ids.facilities().acquire();
        const double x = static_cast<double>(i % kGrid) * kPitch;
        const double z = static_cast<double>(i / kGrid) * kPitch;
        facilities.append({id, {i + 1ULL}, 0, {x, 0, z}, {{x - 2, 0, z - 2}, {x + 2, 5, z + 2}}, true});
    }
    return facilities;
}

[[nodiscard]] PedestrianPathTable gridPaths() {
    const auto nodeKey = [](const std::uint32_t ix, const std::uint32_t iz) {
        return 1ULL + (ix * (kGrid + 1)) + iz;
    };
    std::vector<PedestrianNodeRow> nodes;
    std::vector<PedestrianEdgeRow> edges;
    for (std::uint32_t ix = 0; ix <= kGrid; ++ix) {
        for (std::uint32_t iz = 0; iz <= kGrid; ++iz) {
            nodes.push_back({nodeKey(ix, iz),
                             {(ix * kPitch) - (kPitch / 2), 0.0, (iz * kPitch) - (kPitch / 2)}});
            const std::uint64_t base = 2ULL * nodeKey(ix, iz);
            if (ix < kGrid) {
                edges.push_back({base, nodeKey(ix, iz), nodeKey(ix + 1, iz), kPitch});
            }
            if (iz < kGrid) {
                edges.push_back({base + 1, nodeKey(ix, iz), nodeKey(ix, iz + 1), kPitch});
            }
        }
    }
    std::vector<PedestrianEntranceRow> entrances;
    for (std::uint32_t i = 0; i < kGrid * kGrid; ++i) {
        const std::uint32_t ix = i % kGrid;
        const std::uint32_t iz = i / kGrid;
        entrances.push_back({{i + 1ULL},
                             {ix * kPitch, 0.0, (iz * kPitch) - 2.0},
                             nodeKey(ix, iz)});
    }
    return PedestrianPathTable::build(nodes, edges, entrances);
}

[[nodiscard]] FirstPlayableContent gameplayContent() {
    auto content = konbini::sim::loadFirstPlayableContent(KONBINI_PHASE1_CONTENT_FILE);
    // Shorten the match; the ramp must fit inside it to pass content
    // validation (phase1.json ramps over 2400 ticks).
    content.phase1->durationTicks = 120;
    content.phase1->aiRampTicks = 80;
    content.phase1->aiOpeningPeriodTicks = 4;
    content.phase1->aiPeriodTicks = 2;
    return content;
}

void testPathsDoNotChangeGameplayState() {
    const FirstPlayableContent content = gameplayContent();
    WorldEntityIds plainIds;
    WorldEntityIds pathIds;
    // Build the facilities first: argument evaluation order is unspecified,
    // and the id pools must be copied after the facility ids are acquired.
    FacilityTable plainFacilities = gridFacilities(plainIds);
    FacilityTable pathFacilities = gridFacilities(pathIds);
    FirstPlayableSimulation plain(content, std::move(plainFacilities), plainIds);
    FirstPlayableSimulation withPaths(
        content, std::move(pathFacilities), pathIds, gridPaths());
    CHECK(plain.pedestrianPaths() == nullptr);
    CHECK(withPaths.pedestrianPaths() != nullptr);

    const SelectChainCommand select{{0, CommandSourcePriority::Player, 0}, ChainId::Losan};
    plain.submit(select);
    withPaths.submit(select);
    bool sawPathResident = false;
    for (std::uint32_t tick = 0; tick < 60; ++tick) {
        const auto left = plain.completeNextTick();
        const auto right = withPaths.completeNextTick();
        CHECK(left.canonical.hash == right.canonical.hash);
        CHECK(left.canonical.bytes == right.canonical.bytes);
        CHECK(left.render->hud().totalPopulation == right.render->hud().totalPopulation);
        CHECK(left.render->hud().cashCredits == right.render->hud().cashCredits);
        CHECK(left.render->hud().predictedEconomyIncomeCredits ==
              right.render->hud().predictedEconomyIncomeCredits);
        for (const ResidentPresentation& resident : right.render->residents()) {
            CHECK(resident.route != ResidentRouteState::DirectLine);
            sawPathResident = sawPathResident ||
                              resident.route == ResidentRouteState::PedestrianPath;
        }
    }
    // The opening AI stores attract residents, so the path branch is exercised.
    CHECK(sawPathResident);
}

}  // namespace

int main() {
    testTableRejectsMalformedInput();
    testEqualLengthTieBreakFollowsStableEdgeKey();
    testShorterRouteBeatsSmallerKeys();
    testUnreachableAndMissingEntranceAreExplicit();
    testResidentWalksAlongTheSelectedPath();
    testUnreachableResidentStaysHomeWithoutFallback();
    testMissingEntranceIsExplicit();
    testWithoutNetworkKeepsTheStraightLineBaseline();
    testPathsDoNotChangeGameplayState();
    return konbini::test::summarize("konbini_pedestrian_path_walking_tests");
}
