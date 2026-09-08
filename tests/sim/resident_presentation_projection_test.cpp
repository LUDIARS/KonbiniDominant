#include "konbini/sim/resident_presentation.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "konbini/sim/counter_rng.h"

#include "../check.h"

// @implements spec/test/verification-strategy.md 2. Deterministic simulation
// @implements spec/feature/npc-conversations-and-placement-feedback.md Ambient resident baseline
// @implements spec/feature/npc-conversations-and-placement-feedback.md Determinism and ownership

namespace {

using konbini::sim::ChainId;
using konbini::sim::EntityId;
using konbini::sim::FacilityId;
using konbini::sim::PopulationCellId;
using konbini::sim::PopulationCellRow;
using konbini::sim::PopulationCellTable;
using konbini::sim::RandomStreamId;
using konbini::sim::ResidentPresentation;
using konbini::sim::ResidentPresentationContent;
using konbini::sim::ResidentTripPhase;
using konbini::sim::StoreId;
using konbini::sim::StoreRow;
using konbini::sim::StoreTable;
using konbini::sim::Vec3;
using konbini::sim::counterRandom;
using konbini::sim::kResidentRemarkCount;
using konbini::sim::projectResidentPresentations;

constexpr std::uint64_t kWorldSeed = 42;
constexpr std::uint32_t kTicksPerSecond = 2;

// A short cycle keeps every phase boundary reachable as an exact tick index
// while still exercising the same arithmetic as the shipped content profile.
constexpr std::uint64_t kHomeDwellTicks = 4;
constexpr std::uint64_t kStoreDwellTicks = 6;
constexpr std::uint64_t kSpeechDurationTicks = 2;
constexpr std::uint64_t kTravelTicks = 12;
constexpr std::uint64_t kStoreStartTick = kHomeDwellTicks + kTravelTicks;
constexpr std::uint64_t kReturnStartTick = kStoreStartTick + kStoreDwellTicks;
constexpr std::uint64_t kCycleTicks = kReturnStartTick + kTravelTicks;

const Vec3 kHomeMeters{0.0, 0.5, 0.0};
const Vec3 kStoreMeters{12.0, 0.0, 0.0};
const Vec3 kDestinationMeters{12.0, 0.5, 0.0};

const PopulationCellId kCellId{EntityId{7, 1}};
const FacilityId kFacilityId{EntityId{3, 1}};
const StoreId kStoreId{EntityId{2, 1}};

[[nodiscard]] ResidentPresentationContent makeContent() {
    return {
        .samplesPerPopulationCell = 1,
        .walkingSpeedMetersPerSecond = 2.0,
        .homeDwellTicks = static_cast<std::uint32_t>(kHomeDwellTicks),
        .storeDwellTicks = static_cast<std::uint32_t>(kStoreDwellTicks),
        .speechDurationTicks =
            static_cast<std::uint32_t>(kSpeechDurationTicks),
        .bubbleHeightMeters = 2.2,
        .bubbleMaxDistanceMeters = 50.0,
        .remarks = {{std::string("ALPHA"), std::string("BETA"),
                     std::string("GAMMA")}},
    };
}

[[nodiscard]] StoreRow makeStoreRow() {
    return {
        .id = kStoreId,
        .facilityId = kFacilityId,
        .chain = ChainId::Losan,
        .dimension = 0,
        .positionMeters = kStoreMeters,
        .zocRadiusMeters = 18.0,
        .capturedPopulation = 0,
        .isActive = true,
    };
}

[[nodiscard]] PopulationCellRow makeCellRow() {
    return {
        .id = kCellId,
        .facilityId = kFacilityId,
        .dimension = 0,
        .positionMeters = kHomeMeters,
        .population = 10,
        .assignedStore = kStoreId,
        .preferredChain = ChainId::Losan,
    };
}

[[nodiscard]] StoreTable makeStores(const StoreRow& row) {
    StoreTable stores;
    stores.append(row);
    return stores;
}

[[nodiscard]] PopulationCellTable makeCells(const PopulationCellRow& row) {
    PopulationCellTable cells;
    cells.append(row);
    return cells;
}

// --- mirrors of the projection's own derivations -----------------------
// These recompute the schedule the way the projection does. Re-deriving the
// value is the point: the assertions below pin the observable record against
// the documented rule, not against a captured output blob.

[[nodiscard]] std::uint64_t stableEntityValue(const PopulationCellId id) {
    const EntityId value = id.value();
    return (static_cast<std::uint64_t>(value.generation) << 32U) | value.index;
}

[[nodiscard]] std::uint64_t phaseOffsetFor(const std::uint64_t worldSeed,
                                           const PopulationCellId cellId,
                                           const std::uint32_t ordinal,
                                           const std::uint64_t cycleTicks) {
    return counterRandom({
               .worldSeed = worldSeed,
               .stream = RandomStreamId::FirstPlayableResidentSchedule,
               .tick = 0,
               .stableId = stableEntityValue(cellId),
               .ordinal = ordinal,
           }) %
           cycleTicks;
}

[[nodiscard]] std::size_t remarkIndexFor(const std::uint64_t worldSeed,
                                         const PopulationCellId cellId,
                                         const std::uint32_t ordinal) {
    return static_cast<std::size_t>(
        counterRandom({
            .worldSeed = worldSeed,
            .stream = RandomStreamId::FirstPlayableResidentRemark,
            .tick = 0,
            .stableId = stableEntityValue(cellId),
            .ordinal = ordinal,
        }) %
        kResidentRemarkCount);
}

// Inverse of the projection's cycle mapping: the completed-tick count that
// places the resident at `cycleTick` inside its own schedule.
[[nodiscard]] std::uint64_t completedTicksForCycleTick(
    const std::uint64_t cycleTick, const std::uint64_t phaseOffset,
    const std::uint64_t cycleTicks) {
    return (cycleTick + cycleTicks - phaseOffset) % cycleTicks;
}

[[nodiscard]] double interpolatedAxis(const double start, const double end,
                                      const std::uint64_t elapsedTicks,
                                      const std::uint64_t durationTicks) {
    const double progress = static_cast<double>(elapsedTicks) /
                            static_cast<double>(durationTicks);
    return start + ((end - start) * progress);
}

[[nodiscard]] bool sameVec3(const Vec3 left, const Vec3 right) {
    return left.x == right.x && left.y == right.y && left.z == right.z;
}

[[nodiscard]] bool sameResident(const ResidentPresentation& left,
                                const ResidentPresentation& right) {
    return left.id == right.id && left.phase == right.phase &&
           sameVec3(left.positionMeters, right.positionMeters) &&
           left.yawRadians == right.yawRadians &&
           left.targetStore == right.targetStore &&
           left.speech == right.speech &&
           left.bubble.heightMeters == right.bubble.heightMeters &&
           left.bubble.maxDistanceMeters == right.bubble.maxDistanceMeters;
}

[[nodiscard]] ResidentPresentation projectAtCycleTick(
    const std::uint64_t cycleTick) {
    const ResidentPresentationContent content = makeContent();
    const StoreTable stores = makeStores(makeStoreRow());
    const PopulationCellTable cells = makeCells(makeCellRow());
    const std::uint64_t offset =
        phaseOffsetFor(kWorldSeed, kCellId, 0, kCycleTicks);
    const std::vector<ResidentPresentation> residents =
        projectResidentPresentations(
            completedTicksForCycleTick(cycleTick, offset, kCycleTicks),
            kWorldSeed, kTicksPerSecond, content, cells, stores);
    if (residents.size() != 1) {
        CHECK(residents.size() == 1);
        return {};
    }
    return residents.front();
}

// --- unassigned --------------------------------------------------------

// A cell with no assigned store has no destination, so it must stay at home
// instead of being given an arbitrary one.
void testUnassignedCellStaysAtHome() {
    const ResidentPresentationContent content = makeContent();
    const StoreTable stores = makeStores(makeStoreRow());
    PopulationCellRow row = makeCellRow();
    row.assignedStore.reset();
    row.preferredChain.reset();
    const PopulationCellTable cells = makeCells(row);

    for (const std::uint64_t tick : {std::uint64_t{0}, std::uint64_t{5},
                                     std::uint64_t{97},
                                     std::uint64_t{1000003}}) {
        const std::vector<ResidentPresentation> residents =
            projectResidentPresentations(tick, kWorldSeed, kTicksPerSecond,
                                         content, cells, stores);
        CHECK(residents.size() == 1);
        if (residents.size() != 1) {
            continue;
        }
        const ResidentPresentation& resident = residents.front();
        CHECK(resident.id.populationCellId == kCellId);
        CHECK(resident.id.ordinal == 0);
        CHECK(resident.phase == ResidentTripPhase::AtHome);
        CHECK(sameVec3(resident.positionMeters, kHomeMeters));
        CHECK(resident.yawRadians == 0.0);
        CHECK(!resident.targetStore.has_value());
        CHECK(!resident.speech.has_value());
        CHECK(resident.bubble.heightMeters == content.bubbleHeightMeters);
        CHECK(resident.bubble.maxDistanceMeters ==
              content.bubbleMaxDistanceMeters);
    }
}

// --- determinism -------------------------------------------------------

void testProjectionIsDeterministic() {
    const ResidentPresentationContent content = makeContent();
    const StoreTable storesA = makeStores(makeStoreRow());
    const PopulationCellTable cellsA = makeCells(makeCellRow());
    const StoreTable storesB = makeStores(makeStoreRow());
    const PopulationCellTable cellsB = makeCells(makeCellRow());

    for (const std::uint64_t tick :
         {std::uint64_t{0}, std::uint64_t{1}, std::uint64_t{17},
          std::uint64_t{34}, std::uint64_t{9999}}) {
        const std::vector<ResidentPresentation> first =
            projectResidentPresentations(tick, kWorldSeed, kTicksPerSecond,
                                         content, cellsA, storesA);
        const std::vector<ResidentPresentation> second =
            projectResidentPresentations(tick, kWorldSeed, kTicksPerSecond,
                                         content, cellsA, storesA);
        // Separately built tables with the same rows must project the same
        // records: the projection may not depend on allocation history.
        const std::vector<ResidentPresentation> third =
            projectResidentPresentations(tick, kWorldSeed, kTicksPerSecond,
                                         content, cellsB, storesB);

        CHECK(first.size() == second.size());
        CHECK(first.size() == third.size());
        for (std::size_t index = 0;
             index < first.size() && index < second.size() &&
             index < third.size();
             ++index) {
            CHECK(sameResident(first[index], second[index]));
            CHECK(sameResident(first[index], third[index]));
        }
    }
}

// The schedule repeats exactly, so the same point in the cycle one or many
// cycles later must produce a bit-identical record.
void testCycleRepeatsExactly() {
    const ResidentPresentationContent content = makeContent();
    const StoreTable stores = makeStores(makeStoreRow());
    const PopulationCellTable cells = makeCells(makeCellRow());

    for (const std::uint64_t tick : {std::uint64_t{0}, std::uint64_t{3},
                                     std::uint64_t{20}, std::uint64_t{33}}) {
        const std::vector<ResidentPresentation> base =
            projectResidentPresentations(tick, kWorldSeed, kTicksPerSecond,
                                         content, cells, stores);
        const std::vector<ResidentPresentation> later =
            projectResidentPresentations(tick + (kCycleTicks * 7), kWorldSeed,
                                         kTicksPerSecond, content, cells,
                                         stores);
        CHECK(base.size() == later.size());
        for (std::size_t index = 0;
             index < base.size() && index < later.size(); ++index) {
            CHECK(sameResident(base[index], later[index]));
        }
    }
}

void testRemarkFollowsItsOwnStream() {
    const ResidentPresentationContent content = makeContent();
    const ResidentPresentation resident = projectAtCycleTick(kStoreStartTick);
    CHECK(resident.speech.has_value());
    if (resident.speech.has_value()) {
        CHECK(*resident.speech ==
              content.remarks[remarkIndexFor(kWorldSeed, kCellId, 0)]);
    }

    // The remark is drawn from its own counter stream, so it must not change
    // with the tick even though the phase does.
    const ResidentPresentation later =
        projectAtCycleTick(kStoreStartTick + kSpeechDurationTicks - 1);
    CHECK(later.speech == resident.speech);
}

// --- phase boundaries --------------------------------------------------

void testHomeAndWalkingBoundaries() {
    const ResidentPresentation lastHomeTick =
        projectAtCycleTick(kHomeDwellTicks - 1);
    CHECK(lastHomeTick.phase == ResidentTripPhase::AtHome);
    CHECK(sameVec3(lastHomeTick.positionMeters, kHomeMeters));
    CHECK(lastHomeTick.targetStore.has_value());
    CHECK(lastHomeTick.targetStore == kStoreId);
    CHECK(!lastHomeTick.speech.has_value());
    // Residents face their destination while they wait, so the outbound yaw
    // is already applied during the home dwell.
    CHECK(lastHomeTick.yawRadians ==
          std::atan2(kDestinationMeters.x - kHomeMeters.x,
                     kDestinationMeters.z - kHomeMeters.z));

    const ResidentPresentation firstWalkTick =
        projectAtCycleTick(kHomeDwellTicks);
    CHECK(firstWalkTick.phase == ResidentTripPhase::WalkingToStore);
    // The first walking tick has zero elapsed travel, so it must still be
    // exactly at home rather than one step along the path.
    CHECK(sameVec3(firstWalkTick.positionMeters, kHomeMeters));
    CHECK(!firstWalkTick.speech.has_value());

    const ResidentPresentation midWalkTick =
        projectAtCycleTick(kHomeDwellTicks + 5);
    CHECK(midWalkTick.phase == ResidentTripPhase::WalkingToStore);
    CHECK(midWalkTick.positionMeters.x ==
          interpolatedAxis(kHomeMeters.x, kDestinationMeters.x, 5,
                           kTravelTicks));
    CHECK(midWalkTick.positionMeters.z ==
          interpolatedAxis(kHomeMeters.z, kDestinationMeters.z, 5,
                           kTravelTicks));
    // Walking never changes altitude: the height stays the cell's own.
    CHECK(midWalkTick.positionMeters.y == kHomeMeters.y);

    const ResidentPresentation lastWalkTick =
        projectAtCycleTick(kStoreStartTick - 1);
    CHECK(lastWalkTick.phase == ResidentTripPhase::WalkingToStore);
    CHECK(lastWalkTick.positionMeters.x ==
          interpolatedAxis(kHomeMeters.x, kDestinationMeters.x,
                           kTravelTicks - 1, kTravelTicks));
    CHECK(!sameVec3(lastWalkTick.positionMeters, kDestinationMeters));
}

void testStoreDwellAndSpeechBoundaries() {
    const ResidentPresentation arrivalTick =
        projectAtCycleTick(kStoreStartTick);
    CHECK(arrivalTick.phase == ResidentTripPhase::AtStore);
    CHECK(sameVec3(arrivalTick.positionMeters, kDestinationMeters));
    CHECK(arrivalTick.speech.has_value());

    const ResidentPresentation lastSpeakingTick =
        projectAtCycleTick(kStoreStartTick + kSpeechDurationTicks - 1);
    CHECK(lastSpeakingTick.phase == ResidentTripPhase::AtStore);
    CHECK(lastSpeakingTick.speech.has_value());

    // Speech ends before the dwell does; the resident stays at the store.
    const ResidentPresentation firstSilentTick =
        projectAtCycleTick(kStoreStartTick + kSpeechDurationTicks);
    CHECK(firstSilentTick.phase == ResidentTripPhase::AtStore);
    CHECK(!firstSilentTick.speech.has_value());
    CHECK(sameVec3(firstSilentTick.positionMeters, kDestinationMeters));

    const ResidentPresentation lastDwellTick =
        projectAtCycleTick(kReturnStartTick - 1);
    CHECK(lastDwellTick.phase == ResidentTripPhase::AtStore);
    CHECK(!lastDwellTick.speech.has_value());
    CHECK(sameVec3(lastDwellTick.positionMeters, kDestinationMeters));
}

void testReturnBoundaries() {
    const ResidentPresentation firstReturnTick =
        projectAtCycleTick(kReturnStartTick);
    CHECK(firstReturnTick.phase == ResidentTripPhase::WalkingHome);
    CHECK(sameVec3(firstReturnTick.positionMeters, kDestinationMeters));
    CHECK(!firstReturnTick.speech.has_value());
    CHECK(firstReturnTick.yawRadians ==
          std::atan2(kHomeMeters.x - kDestinationMeters.x,
                     kHomeMeters.z - kDestinationMeters.z));

    const ResidentPresentation midReturnTick =
        projectAtCycleTick(kReturnStartTick + 5);
    CHECK(midReturnTick.phase == ResidentTripPhase::WalkingHome);
    CHECK(midReturnTick.positionMeters.x ==
          interpolatedAxis(kDestinationMeters.x, kHomeMeters.x, 5,
                           kTravelTicks));

    const ResidentPresentation lastReturnTick =
        projectAtCycleTick(kCycleTicks - 1);
    CHECK(lastReturnTick.phase == ResidentTripPhase::WalkingHome);
    CHECK(!sameVec3(lastReturnTick.positionMeters, kHomeMeters));

    // Wrapping past the end of the cycle returns to the home dwell rather
    // than overrunning into a second return leg.
    const ResidentPresentation wrappedTick = projectAtCycleTick(0);
    CHECK(wrappedTick.phase == ResidentTripPhase::AtHome);
    CHECK(sameVec3(wrappedTick.positionMeters, kHomeMeters));
}

// --- zero distance -----------------------------------------------------

// A resident whose assigned store sits on its own cell has no direction to
// face and no travel to interpolate, but the trip must still advance one tick
// per leg instead of dividing by a zero-length walk.
void testZeroDistanceTripKeepsOneTickLegs() {
    const ResidentPresentationContent content = makeContent();
    const Vec3 sharedPosition{5.0, 0.5, 7.0};
    StoreRow storeRow = makeStoreRow();
    storeRow.positionMeters = {5.0, 0.0, 7.0};
    PopulationCellRow cellRow = makeCellRow();
    cellRow.positionMeters = sharedPosition;
    const StoreTable stores = makeStores(storeRow);
    const PopulationCellTable cells = makeCells(cellRow);

    constexpr std::uint64_t kShortTravelTicks = 1;
    constexpr std::uint64_t kShortStoreStart =
        kHomeDwellTicks + kShortTravelTicks;
    constexpr std::uint64_t kShortReturnStart =
        kShortStoreStart + kStoreDwellTicks;
    constexpr std::uint64_t kShortCycle = kShortReturnStart + kShortTravelTicks;
    const std::uint64_t offset =
        phaseOffsetFor(kWorldSeed, kCellId, 0, kShortCycle);

    struct Expectation {
        std::uint64_t cycleTick;
        ResidentTripPhase phase;
    };
    const Expectation expectations[] = {
        {kHomeDwellTicks - 1, ResidentTripPhase::AtHome},
        {kHomeDwellTicks, ResidentTripPhase::WalkingToStore},
        {kShortStoreStart, ResidentTripPhase::AtStore},
        {kShortReturnStart - 1, ResidentTripPhase::AtStore},
        {kShortReturnStart, ResidentTripPhase::WalkingHome},
        {kShortCycle - 1, ResidentTripPhase::WalkingHome},
    };

    for (const Expectation& expectation : expectations) {
        const std::vector<ResidentPresentation> residents =
            projectResidentPresentations(
                completedTicksForCycleTick(expectation.cycleTick, offset,
                                           kShortCycle),
                kWorldSeed, kTicksPerSecond, content, cells, stores);
        CHECK(residents.size() == 1);
        if (residents.size() != 1) {
            continue;
        }
        const ResidentPresentation& resident = residents.front();
        CHECK(resident.phase == expectation.phase);
        // Every position along a zero-length trip is the shared position.
        CHECK(sameVec3(resident.positionMeters, sharedPosition));
        // No direction of travel means no facing change in either leg.
        CHECK(resident.yawRadians == 0.0);
    }
}

// --- ordering and sampling ---------------------------------------------

void testRecordsAreOrderedByCellThenOrdinal() {
    ResidentPresentationContent content = makeContent();
    content.samplesPerPopulationCell = 3;

    const PopulationCellId lowerCellId{EntityId{1, 1}};
    const PopulationCellId higherCellId{EntityId{6, 1}};
    PopulationCellRow higherRow = makeCellRow();
    higherRow.id = higherCellId;
    PopulationCellRow lowerRow = makeCellRow();
    lowerRow.id = lowerCellId;
    lowerRow.assignedStore.reset();
    lowerRow.preferredChain.reset();

    PopulationCellTable cells;
    // Appended highest-first so that a projection that leaned on insertion
    // order would produce the wrong sequence.
    cells.append(higherRow);
    cells.append(lowerRow);
    const StoreTable stores = makeStores(makeStoreRow());

    const std::vector<ResidentPresentation> residents =
        projectResidentPresentations(11, kWorldSeed, kTicksPerSecond, content,
                                     cells, stores);
    CHECK(residents.size() == 6);
    if (residents.size() != 6) {
        return;
    }
    for (std::size_t ordinal = 0; ordinal < 3; ++ordinal) {
        const std::uint32_t expectedOrdinal =
            static_cast<std::uint32_t>(ordinal);
        CHECK(residents[ordinal].id.populationCellId == lowerCellId);
        CHECK(residents[ordinal].id.ordinal == expectedOrdinal);
        CHECK(residents[ordinal + 3].id.populationCellId == higherCellId);
        CHECK(residents[ordinal + 3].id.ordinal == expectedOrdinal);
    }
}

// --- rejected inputs ---------------------------------------------------

void testInvalidInputsAreRejected() {
    const ResidentPresentationContent content = makeContent();
    const StoreTable stores = makeStores(makeStoreRow());
    const PopulationCellTable cells = makeCells(makeCellRow());

    CHECK_THROWS(std::invalid_argument,
                 (void)projectResidentPresentations(0, kWorldSeed, 0, content,
                                                    cells, stores));

    ResidentPresentationContent invalidContent = content;
    invalidContent.speechDurationTicks =
        invalidContent.storeDwellTicks + 1;
    CHECK_THROWS(std::invalid_argument,
                 (void)projectResidentPresentations(
                     0, kWorldSeed, kTicksPerSecond, invalidContent, cells,
                     stores));

    // An assignment that cannot be resolved to an active local store is a
    // table inconsistency, not a resident that quietly stays home.
    const StoreTable emptyStores;
    CHECK_THROWS(std::logic_error,
                 (void)projectResidentPresentations(
                     0, kWorldSeed, kTicksPerSecond, content, cells,
                     emptyStores));

    PopulationCellRow staleRow = makeCellRow();
    staleRow.assignedStore = StoreId{EntityId{2, 2}};
    const PopulationCellTable staleCells = makeCells(staleRow);
    CHECK_THROWS(std::logic_error,
                 (void)projectResidentPresentations(
                     0, kWorldSeed, kTicksPerSecond, content, staleCells,
                     stores));

    StoreRow inactiveRow = makeStoreRow();
    inactiveRow.isActive = false;
    const StoreTable inactiveStores = makeStores(inactiveRow);
    CHECK_THROWS(std::logic_error,
                 (void)projectResidentPresentations(
                     0, kWorldSeed, kTicksPerSecond, content, cells,
                     inactiveStores));

    StoreRow otherDimensionRow = makeStoreRow();
    otherDimensionRow.dimension = 1;
    const StoreTable otherDimensionStores = makeStores(otherDimensionRow);
    CHECK_THROWS(std::logic_error,
                 (void)projectResidentPresentations(
                     0, kWorldSeed, kTicksPerSecond, content, cells,
                     otherDimensionStores));

    PopulationCellRow otherChainRow = makeCellRow();
    otherChainRow.preferredChain = ChainId::Famoma;
    const PopulationCellTable otherChainCells = makeCells(otherChainRow);
    CHECK_THROWS(std::logic_error,
                 (void)projectResidentPresentations(
                     0, kWorldSeed, kTicksPerSecond, content, otherChainCells,
                     stores));
}

}  // namespace

int main() {
    testUnassignedCellStaysAtHome();
    testProjectionIsDeterministic();
    testCycleRepeatsExactly();
    testRemarkFollowsItsOwnStream();
    testHomeAndWalkingBoundaries();
    testStoreDwellAndSpeechBoundaries();
    testReturnBoundaries();
    testZeroDistanceTripKeepsOneTickLegs();
    testRecordsAreOrderedByCellThenOrdinal();
    testInvalidInputsAreRejected();
    return konbini::test::summarize("resident_presentation_projection_test");
}
