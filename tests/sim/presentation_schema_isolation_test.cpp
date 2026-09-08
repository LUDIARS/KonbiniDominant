#include "konbini/sim/canonical_snapshot.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "konbini/sim/render_snapshot.h"

#include "../check.h"

// @implements spec/test/verification-strategy.md 2. Deterministic simulation
// @implements spec/feature/npc-conversations-and-placement-feedback.md Determinism and ownership
// @implements spec/interface/pictor-rendering.md Ownership

namespace {

using konbini::sim::CanonicalSnapshot;
using konbini::sim::ChainContent;
using konbini::sim::ChainEconomyTable;
using konbini::sim::ChainId;
using konbini::sim::EntityId;
using konbini::sim::FacilityId;
using konbini::sim::FacilityRow;
using konbini::sim::FacilityState;
using konbini::sim::FacilityTable;
using konbini::sim::FigmentumFacilityKey;
using konbini::sim::FirstPlayableContent;
using konbini::sim::GamePhase;
using konbini::sim::GameState;
using konbini::sim::PopulationCellId;
using konbini::sim::PopulationCellRow;
using konbini::sim::PopulationCellTable;
using konbini::sim::RenderSnapshot;
using konbini::sim::RenderStorePlacementCue;
using konbini::sim::StoreId;
using konbini::sim::StoreRow;
using konbini::sim::StoreTable;
using konbini::sim::Vec3;
using konbini::sim::kCanonicalSnapshotSchemaVersion;
using konbini::sim::kFirstPlayableChainCount;
using konbini::sim::makeCanonicalSnapshot;
using konbini::sim::makeRenderSnapshot;

// The canonical byte layout written by makeCanonicalSnapshot, expressed as
// the size of each record it is allowed to contain. Presentation records have
// no entry here on purpose: if a resident or a placement cue ever reaches the
// canonical stream, the measured payload stops matching this sum.
constexpr std::size_t kU8 = 1;
constexpr std::size_t kU32 = 4;
constexpr std::size_t kU64 = 8;
constexpr std::size_t kEntity = kU32 + kU32;
constexpr std::size_t kF64 = 8;
constexpr std::size_t kVec3 = 3 * kF64;
constexpr std::size_t kBounds = 2 * kVec3;
constexpr std::size_t kMagicText = kU32 + 32;

constexpr std::size_t kHeaderBytes =
    kMagicText + kU32 + kU32 + kU32 + kU64 + kU8 + kU8 + kU8 + kU64;
constexpr std::size_t kFacilityBytes =
    kEntity + kU64 + kU32 + kVec3 + kBounds + kU8 + kU8;
constexpr std::size_t kStoreBytes =
    kEntity + kEntity + kU8 + kU32 + kVec3 + kF64 + kU64 + kU8;
constexpr std::size_t kPopulationBytes =
    kEntity + kEntity + kU32 + kVec3 + kU32 + kU8;
constexpr std::size_t kPopulationAssignmentBytes = kEntity + kU8;
constexpr std::size_t kChainEconomyBytes =
    kU8 + kU64 + kU32 + kU64 + kU64 + kU64 + kU8;

const FacilityId kFacilityId{EntityId{2, 1}};
const StoreId kStoreId{EntityId{5, 1}};
const PopulationCellId kCellId{EntityId{8, 1}};

[[nodiscard]] ChainContent makeChain(const ChainId id,
                                     const std::string& displayName,
                                     const std::int64_t buildCost,
                                     const double radius,
                                     const std::int64_t revenueMilli) {
    return {
        .id = id,
        .displayName = displayName,
        .buildCostCredits = buildCost,
        .zocRadiusMeters = radius,
        .revenueMilliCreditsPerPerson = revenueMilli,
    };
}

[[nodiscard]] FirstPlayableContent makeContent() {
    FirstPlayableContent content;
    content.schemaVersion = 1;
    content.contentVersion = 2;
    content.simulation = {
        .ticksPerSecond = 10,
        .economyPeriodTicks = 10,
        .randomAlgorithm = "splitmix64-counter-v1",
    };
    content.population = {
        .basePopulation = 50,
        .randomPopulationCount = 101,
        .randomStream = "FP_POPULATION",
    };
    content.residentPresentation = {
        .samplesPerPopulationCell = 1,
        .walkingSpeedMetersPerSecond = 1.5,
        .homeDwellTicks = 30,
        .storeDwellTicks = 40,
        .speechDurationTicks = 30,
        .bubbleHeightMeters = 2.2,
        .bubbleMaxDistanceMeters = 220.0,
        .remarks = {{std::string("NICE AND CLOSE"),
                     std::string("EASY TO REACH"),
                     std::string("HANDY LOCATION")}},
    };
    content.startingStoreEquivalent = 5;
    content.chains = {{
        makeChain(ChainId::Losan, "Losan", 1000, 18.0, 500),
        makeChain(ChainId::Famoma, "Famoma", 1250, 24.0, 450),
        makeChain(ChainId::SebanIleban, "Seban Ileban", 800, 18.0, 400),
    }};
    return content;
}

// A second presentation profile that changes every resident-facing value.
// Nothing here may reach the canonical stream.
[[nodiscard]] FirstPlayableContent makeContentWithOtherResidentProfile() {
    FirstPlayableContent content = makeContent();
    content.residentPresentation = {
        .samplesPerPopulationCell = 4,
        .walkingSpeedMetersPerSecond = 0.75,
        .homeDwellTicks = 3,
        .storeDwellTicks = 5,
        .speechDurationTicks = 1,
        .bubbleHeightMeters = 9.5,
        .bubbleMaxDistanceMeters = 12.5,
        .remarks = {{std::string("ALPHA"), std::string("BETA"),
                     std::string("GAMMA")}},
    };
    return content;
}

[[nodiscard]] GameState makeState() {
    return {
        .completedTicks = 77,
        .phase = GamePhase::Phase1,
        .playerChain = ChainId::Losan,
        .worldSeed = 42,
    };
}

[[nodiscard]] FacilityTable makeFacilities() {
    FacilityTable facilities;
    facilities.append({
        .id = kFacilityId,
        .figmentumKey = FigmentumFacilityKey{9001},
        .dimension = 0,
        .positionMeters = {4.0, 0.0, 6.0},
        .boundsMeters = {{2.0, 0.0, 4.0}, {6.0, 5.0, 8.0}},
        .isBuildable = true,
        .state = FacilityState::Intact,
    });
    return facilities;
}

[[nodiscard]] StoreTable makeStores() {
    StoreTable stores;
    stores.append({
        .id = kStoreId,
        .facilityId = kFacilityId,
        .chain = ChainId::Losan,
        .dimension = 0,
        .positionMeters = {4.0, 0.0, 6.0},
        .zocRadiusMeters = 18.0,
        .capturedPopulation = 25,
        .isActive = true,
    });
    return stores;
}

[[nodiscard]] PopulationCellTable makeAssignedCells() {
    PopulationCellTable cells;
    cells.append({
        .id = kCellId,
        .facilityId = kFacilityId,
        .dimension = 0,
        .positionMeters = {10.0, 0.0, 6.0},
        .population = 25,
        .assignedStore = kStoreId,
        .preferredChain = ChainId::Losan,
    });
    return cells;
}

[[nodiscard]] PopulationCellTable makeUnassignedCells() {
    PopulationCellTable cells;
    cells.append({
        .id = kCellId,
        .facilityId = kFacilityId,
        .dimension = 0,
        .positionMeters = {10.0, 0.0, 6.0},
        .population = 25,
        .assignedStore = std::nullopt,
        .preferredChain = std::nullopt,
    });
    return cells;
}

[[nodiscard]] std::vector<RenderStorePlacementCue> makeCues() {
    return {
        {
            .storeId = kStoreId,
            .targetPositionMeters = {4.0, 0.0, 6.0},
            .targetYawDegrees = 0.0,
            .completedTick = 77,
        },
    };
}

// --- canonical layout --------------------------------------------------

// The canonical payload is the save / replay identity of a tick. Pinning its
// exact size is how a presentation field added to it stops being invisible.
void testCanonicalLayoutHasNoPresentationRecords() {
    const FirstPlayableContent content = makeContent();
    const ChainEconomyTable economy{content};
    const CanonicalSnapshot assigned = makeCanonicalSnapshot(
        makeState(), content, makeFacilities(), makeStores(),
        makeAssignedCells(), economy);

    const std::size_t expectedAssignedBytes =
        kHeaderBytes + kU64 + kFacilityBytes + kU64 + kStoreBytes + kU64 +
        kPopulationBytes + kPopulationAssignmentBytes + kU32 +
        (kChainEconomyBytes * kFirstPlayableChainCount);

    CHECK(assigned.schemaVersion == kCanonicalSnapshotSchemaVersion);
    CHECK(assigned.bytes.size() == expectedAssignedBytes);

    const CanonicalSnapshot unassigned = makeCanonicalSnapshot(
        makeState(), content, makeFacilities(), makeStores(),
        makeUnassignedCells(), economy);
    // The only difference an unassigned cell may make is its own assignment
    // record; nothing resident-shaped is written for either cell.
    CHECK(unassigned.bytes.size() ==
          expectedAssignedBytes - kPopulationAssignmentBytes);
}

// --- presentation isolation --------------------------------------------

// The resident profile is the widest presentation-only input there is. If any
// of it leaked into canonical state, replays and saves would diverge whenever
// the ambient crowd was retuned.
void testResidentProfileDoesNotChangeCanonicalState() {
    const FirstPlayableContent baselineContent = makeContent();
    const FirstPlayableContent otherContent =
        makeContentWithOtherResidentProfile();
    const ChainEconomyTable baselineEconomy{baselineContent};
    const ChainEconomyTable otherEconomy{otherContent};

    const CanonicalSnapshot baseline = makeCanonicalSnapshot(
        makeState(), baselineContent, makeFacilities(), makeStores(),
        makeAssignedCells(), baselineEconomy);
    const CanonicalSnapshot other = makeCanonicalSnapshot(
        makeState(), otherContent, makeFacilities(), makeStores(),
        makeAssignedCells(), otherEconomy);

    CHECK(baseline.bytes == other.bytes);
    CHECK(baseline.hash == other.hash);

    // The same two profiles must be observable on the render side, otherwise
    // the comparison above proves nothing.
    const std::shared_ptr<const RenderSnapshot> baselineRender =
        makeRenderSnapshot(makeState(), baselineContent, makeFacilities(),
                           makeStores(), makeAssignedCells(), baselineEconomy,
                           std::span<const RenderStorePlacementCue>{});
    const std::shared_ptr<const RenderSnapshot> otherRender =
        makeRenderSnapshot(makeState(), otherContent, makeFacilities(),
                           makeStores(), makeAssignedCells(), otherEconomy,
                           std::span<const RenderStorePlacementCue>{});
    CHECK(baselineRender->residents().size() == 1);
    CHECK(otherRender->residents().size() == 4);
}

// Placement cues are not even an input to the canonical snapshot. This pins
// that: publishing cues on a tick leaves the canonical identity untouched.
void testPlacementCuesDoNotChangeCanonicalState() {
    const FirstPlayableContent content = makeContent();
    const ChainEconomyTable economy{content};
    const std::vector<RenderStorePlacementCue> cues = makeCues();

    const CanonicalSnapshot before = makeCanonicalSnapshot(
        makeState(), content, makeFacilities(), makeStores(),
        makeAssignedCells(), economy);

    const std::shared_ptr<const RenderSnapshot> render = makeRenderSnapshot(
        makeState(), content, makeFacilities(), makeStores(),
        makeAssignedCells(), economy, cues);

    const CanonicalSnapshot after = makeCanonicalSnapshot(
        makeState(), content, makeFacilities(), makeStores(),
        makeAssignedCells(), economy);

    CHECK(render->placementCues().size() == 1);
    CHECK(render->placementCues().front().storeId == kStoreId);
    CHECK(before.bytes == after.bytes);
    CHECK(before.hash == after.hash);
}

// A snapshot without cues and a snapshot with cues share the authoritative
// half exactly; only the presentation spans differ.
void testRenderSnapshotSeparatesAuthoritativeFromPresentation() {
    const FirstPlayableContent content = makeContent();
    const ChainEconomyTable economy{content};

    const std::shared_ptr<const RenderSnapshot> withoutCues =
        makeRenderSnapshot(makeState(), content, makeFacilities(),
                           makeStores(), makeAssignedCells(), economy,
                           std::span<const RenderStorePlacementCue>{});
    const std::vector<RenderStorePlacementCue> cues = makeCues();
    const std::shared_ptr<const RenderSnapshot> withCues = makeRenderSnapshot(
        makeState(), content, makeFacilities(), makeStores(),
        makeAssignedCells(), economy, cues);

    CHECK(withoutCues->placementCues().empty());
    CHECK(withCues->placementCues().size() == 1);
    CHECK(withoutCues->completedTicks() == withCues->completedTicks());
    CHECK(withoutCues->facilities().size() == withCues->facilities().size());
    CHECK(withoutCues->stores().size() == withCues->stores().size());
    CHECK(withoutCues->residents().size() == withCues->residents().size());
    CHECK(withoutCues->hud().completedTicks == withCues->hud().completedTicks);
}

}  // namespace

int main() {
    testCanonicalLayoutHasNoPresentationRecords();
    testResidentProfileDoesNotChangeCanonicalState();
    testPlacementCuesDoNotChangeCanonicalState();
    testRenderSnapshotSeparatesAuthoritativeFromPresentation();
    return konbini::test::summarize("presentation_schema_isolation_test");
}
