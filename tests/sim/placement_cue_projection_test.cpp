#include "konbini/sim/placement_cue_projection.h"

#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

#include "../check.h"

// @implements spec/test/verification-strategy.md 1. Data / ID unit tests
// @implements spec/feature/npc-conversations-and-placement-feedback.md Store placement choreography

namespace {

using konbini::sim::ChainId;
using konbini::sim::EntityId;
using konbini::sim::FacilityId;
using konbini::sim::PlaceStoreCommand;
using konbini::sim::PlacementFailure;
using konbini::sim::PlacementResult;
using konbini::sim::RenderStorePlacementCue;
using konbini::sim::StoreId;
using konbini::sim::StoreRow;
using konbini::sim::StoreTable;
using konbini::sim::Vec3;
using konbini::sim::projectStorePlacementCues;

constexpr std::uint64_t kCompletedTick = 512;

const FacilityId kFacilityId{EntityId{4, 1}};
const StoreId kStoreId{EntityId{1, 1}};
const Vec3 kStoreMeters{31.5, 0.0, -12.25};

[[nodiscard]] StoreRow makeStoreRow() {
    return {
        .id = kStoreId,
        .facilityId = kFacilityId,
        .chain = ChainId::Famoma,
        .dimension = 0,
        .positionMeters = kStoreMeters,
        .zocRadiusMeters = 24.0,
        .capturedPopulation = 0,
        .isActive = true,
    };
}

[[nodiscard]] StoreTable makeStores(const StoreRow& row) {
    StoreTable stores;
    stores.append(row);
    return stores;
}

[[nodiscard]] PlaceStoreCommand makeCommand() {
    return {
        .order = {},
        .chain = ChainId::Famoma,
        .facilityId = kFacilityId,
        .verticalSlot = 0,
    };
}

[[nodiscard]] PlacementResult makeSuccess() {
    return {
        .command = makeCommand(),
        .failure = PlacementFailure::None,
        .placedStore = kStoreId,
    };
}

[[nodiscard]] PlacementResult makeFailure(const PlacementFailure failure) {
    return {
        .command = makeCommand(),
        .failure = failure,
        .placedStore = std::nullopt,
    };
}

// --- successful placements --------------------------------------------

// The cue copies the authoritative transform and starts the animation from a
// canonical zero yaw, so the render side never invents a store orientation.
void testSuccessProducesOneCanonicalCue() {
    const StoreTable stores = makeStores(makeStoreRow());
    const std::array<PlacementResult, 1> placements{makeSuccess()};

    const std::vector<RenderStorePlacementCue> cues =
        projectStorePlacementCues(kCompletedTick, stores, placements);

    CHECK(cues.size() == 1);
    if (cues.size() != 1) {
        return;
    }
    CHECK(cues.front().storeId == kStoreId);
    CHECK(cues.front().targetPositionMeters.x == kStoreMeters.x);
    CHECK(cues.front().targetPositionMeters.y == kStoreMeters.y);
    CHECK(cues.front().targetPositionMeters.z == kStoreMeters.z);
    CHECK(cues.front().targetYawDegrees == 0.0);
    CHECK(cues.front().completedTick == kCompletedTick);
}

// --- failed placements -------------------------------------------------

// Every failure reason must be silent: a rejected placement has no store to
// drop from the sky.
void testFailedPlacementsProduceNoCue() {
    const StoreTable stores = makeStores(makeStoreRow());
    const std::array<PlacementFailure, 10> failures{
        PlacementFailure::WrongPhase,
        PlacementFailure::NoPlayerChain,
        PlacementFailure::InvalidChain,
        PlacementFailure::WrongChain,
        PlacementFailure::UnsupportedVerticalSlot,
        PlacementFailure::FacilityNotFound,
        PlacementFailure::FacilityProtected,
        PlacementFailure::FacilityUnavailable,
        PlacementFailure::FacilityOccupied,
        PlacementFailure::InsufficientCash,
    };

    for (const PlacementFailure failure : failures) {
        const std::array<PlacementResult, 1> placements{makeFailure(failure)};
        const std::vector<RenderStorePlacementCue> cues =
            projectStorePlacementCues(kCompletedTick, stores, placements);
        CHECK(cues.empty());
    }
}

void testOnlySuccessfulPlacementsAreProjected() {
    const StoreTable stores = makeStores(makeStoreRow());
    const std::array<PlacementResult, 4> placements{
        makeFailure(PlacementFailure::InsufficientCash),
        makeSuccess(),
        makeFailure(PlacementFailure::FacilityOccupied),
        makeSuccess(),
    };

    const std::vector<RenderStorePlacementCue> cues =
        projectStorePlacementCues(kCompletedTick, stores, placements);

    CHECK(cues.size() == 2);
    for (const RenderStorePlacementCue& cue : cues) {
        CHECK(cue.storeId == kStoreId);
        CHECK(cue.targetYawDegrees == 0.0);
        CHECK(cue.completedTick == kCompletedTick);
    }
}

void testEmptyInputProducesNoCue() {
    const StoreTable stores = makeStores(makeStoreRow());
    const std::vector<PlacementResult> placements;
    CHECK(projectStorePlacementCues(kCompletedTick, stores, placements)
              .empty());
}

// --- inconsistent results ----------------------------------------------

// A result that both failed and produced a store, or succeeded without one,
// means the command system and the table disagree. Emitting nothing would
// hide that; the projection has to fail instead.
void testInconsistentResultsAreRejected() {
    const StoreTable stores = makeStores(makeStoreRow());

    PlacementResult failedWithStore =
        makeFailure(PlacementFailure::FacilityOccupied);
    failedWithStore.placedStore = kStoreId;
    const std::array<PlacementResult, 1> failedWithStorePlacements{
        failedWithStore};
    CHECK_THROWS(std::logic_error,
                 (void)projectStorePlacementCues(kCompletedTick, stores,
                                                 failedWithStorePlacements));

    PlacementResult successWithoutStore = makeSuccess();
    successWithoutStore.placedStore.reset();
    const std::array<PlacementResult, 1> successWithoutStorePlacements{
        successWithoutStore};
    CHECK_THROWS(std::logic_error,
                 (void)projectStorePlacementCues(
                     kCompletedTick, stores, successWithoutStorePlacements));
}

void testMissingOrStaleStoreIsRejected() {
    const StoreTable stores = makeStores(makeStoreRow());
    const std::array<PlacementResult, 1> placements{makeSuccess()};

    const StoreTable emptyStores;
    CHECK_THROWS(std::logic_error,
                 (void)projectStorePlacementCues(kCompletedTick, emptyStores,
                                                 placements));

    PlacementResult staleResult = makeSuccess();
    staleResult.placedStore = StoreId{EntityId{1, 2}};
    const std::array<PlacementResult, 1> stalePlacements{staleResult};
    CHECK_THROWS(std::logic_error,
                 (void)projectStorePlacementCues(kCompletedTick, stores,
                                                 stalePlacements));
}

void testMismatchedStoreRowIsRejected() {
    const std::array<PlacementResult, 1> placements{makeSuccess()};

    StoreRow inactiveRow = makeStoreRow();
    inactiveRow.isActive = false;
    const StoreTable inactiveStores = makeStores(inactiveRow);
    CHECK_THROWS(std::logic_error,
                 (void)projectStorePlacementCues(kCompletedTick,
                                                 inactiveStores, placements));

    StoreRow otherFacilityRow = makeStoreRow();
    otherFacilityRow.facilityId = FacilityId{EntityId{9, 1}};
    const StoreTable otherFacilityStores = makeStores(otherFacilityRow);
    CHECK_THROWS(std::logic_error,
                 (void)projectStorePlacementCues(
                     kCompletedTick, otherFacilityStores, placements));

    StoreRow otherChainRow = makeStoreRow();
    otherChainRow.chain = ChainId::Losan;
    const StoreTable otherChainStores = makeStores(otherChainRow);
    CHECK_THROWS(std::logic_error,
                 (void)projectStorePlacementCues(kCompletedTick,
                                                 otherChainStores,
                                                 placements));
}

}  // namespace

int main() {
    testSuccessProducesOneCanonicalCue();
    testFailedPlacementsProduceNoCue();
    testOnlySuccessfulPlacementsAreProjected();
    testEmptyInputProducesNoCue();
    testInconsistentResultsAreRejected();
    testMissingOrStaleStoreIsRejected();
    testMismatchedStoreRowIsRejected();
    return konbini::test::summarize("placement_cue_projection_test");
}
