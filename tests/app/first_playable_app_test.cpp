#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "konbini/app/camera_controller.h"
#include "konbini/app/command_composer.h"
#include "konbini/app/fixed_step_driver.h"
#include "konbini/app/frame_input.h"
#include "konbini/app/hud_text_model.h"
#include "konbini/app/selection_controller.h"
#include "konbini/render/bitmap_font.h"
#include "konbini/render/hud_text_geometry.h"
#include "konbini/sim/chain_economy_table.h"
#include "konbini/sim/facility_table.h"
#include "konbini/sim/first_playable_content.h"
#include "konbini/sim/population_cell_table.h"
#include "konbini/sim/population_initializer.h"
#include "konbini/sim/render_snapshot.h"
#include "konbini/sim/store_table.h"
#include "konbini/sim/world_entity_ids.h"

// @implements spec/test/verification-strategy.md 1. Data / ID unit tests
// @implements spec/plan/tasks/first-playable.md Minimal controls

#ifndef KONBINI_TEST_CONTENT_FILE
#error "KONBINI_TEST_CONTENT_FILE must be defined by the build"
#endif

namespace {

int g_failures = 0;

void check(const bool ok, const char* const expression, const int line) {
    if (ok) {
        return;
    }
    ++g_failures;
    std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, line, expression);
}

#define CHECK(expression) check((expression), #expression, __LINE__)

using konbini::app::CameraControlSpec;
using konbini::app::CameraController;
using konbini::app::CommandComposer;
using konbini::app::FixedStepDriver;
using konbini::app::FixedStepPlan;
using konbini::app::FrameInput;
using konbini::app::HudTextInput;
using konbini::app::SelectionController;
using konbini::app::SelectionOutcome;

// --- fixed step ---------------------------------------------------------
// render dt を simulation へ直接積まない、という契約の要。tick 幅と
// catch-up 上限は content と config が決め、超過分は捨てて報告する。

void testFixedStepProducesWholeTicks() {
    FixedStepDriver driver(10, 5);
    CHECK(driver.fixedDeltaSeconds() > 0.099 &&
          driver.fixedDeltaSeconds() < 0.101);

    const FixedStepPlan none = driver.advance(0.05);
    CHECK(none.tickCount == 0);
    CHECK(none.droppedTicks == 0);

    const FixedStepPlan one = driver.advance(0.05);
    CHECK(one.tickCount == 1);
    CHECK(one.droppedTicks == 0);
}

void testFixedStepCapsCatchUpAndReportsDroppedTicks() {
    FixedStepDriver driver(10, 2);
    // 0.25s へ clamp されるので 2 tick 消化 + 0 drop。
    const FixedStepPlan clamped = driver.advance(10.0);
    CHECK(clamped.tickCount == 2);
    CHECK(clamped.droppedTicks == 0);

    FixedStepDriver strict(100, 2);
    const FixedStepPlan dropped = strict.advance(0.25);
    CHECK(dropped.tickCount == 2);
    // 25 tick 相当のうち 2 tick だけ消化し、残りは捨てて報告する。
    CHECK(dropped.droppedTicks == 23);
    CHECK(strict.accumulatedSeconds() < strict.fixedDeltaSeconds());
}

void testFixedStepRejectsInvalidConfiguration() {
    bool threwOnZeroRate = false;
    try {
        FixedStepDriver invalid(0, 1);
        (void)invalid;
    } catch (const std::invalid_argument&) {
        threwOnZeroRate = true;
    }
    CHECK(threwOnZeroRate);

    bool threwOnZeroCatchUp = false;
    try {
        FixedStepDriver invalid(10, 0);
        (void)invalid;
    } catch (const std::invalid_argument&) {
        threwOnZeroCatchUp = true;
    }
    CHECK(threwOnZeroCatchUp);

    FixedStepDriver driver(10, 2);
    bool threwOnNegativeDelta = false;
    try {
        (void)driver.advance(-1.0);
    } catch (const std::invalid_argument&) {
        threwOnNegativeDelta = true;
    }
    CHECK(threwOnNegativeDelta);
}

// --- camera -------------------------------------------------------------

konbini::sim::Bounds3 testCityBounds() {
    return {
        .min = {.x = -100.0, .y = 0.0, .z = -100.0},
        .max = {.x = 100.0, .y = 20.0, .z = 100.0},
    };
}

konbini::render::IsometricCameraConfig testCameraConfig() {
    konbini::render::IsometricCameraConfig config;
    config.targetMeters = {.x = 0.0, .y = 0.0, .z = 0.0};
    config.verticalSpanMeters = 160.0;
    // azimuth 0 にすると screen right が +X、screen forward が +Z になり、
    // pan の期待値を軸単位で書ける。
    config.azimuthDegrees = 0.0;
    return config;
}

void testCameraPanUsesAzimuthBasis() {
    CameraController camera(testCameraConfig(), testCityBounds(), {});
    FrameInput input;
    input.dtSeconds = 0.5;
    input.viewport = {.width = 800, .height = 600};
    input.panRight = 1.0;
    camera.apply(input);
    // 60 m/s * 0.5s = 30m を +X へ。
    CHECK(camera.config().targetMeters.x > 29.9);
    CHECK(camera.config().targetMeters.x < 30.1);
    CHECK(camera.config().targetMeters.z > -0.1);
    CHECK(camera.config().targetMeters.z < 0.1);
}

void testCameraClampsTargetToCityBounds() {
    CameraController camera(testCameraConfig(), testCityBounds(), {});
    FrameInput input;
    input.dtSeconds = 10.0;
    input.viewport = {.width = 800, .height = 600};
    input.panRight = 1.0;
    camera.apply(input);
    const CameraControlSpec& spec = camera.spec();
    CHECK(camera.config().targetMeters.x <=
          testCityBounds().max.x + spec.targetMarginMeters + 1e-9);
}

void testCameraZoomIsClamped() {
    CameraController camera(testCameraConfig(), testCityBounds(), {});
    FrameInput input;
    input.dtSeconds = 0.0;
    input.viewport = {.width = 800, .height = 600};
    input.zoomSteps = 100.0;
    camera.apply(input);
    CHECK(camera.config().verticalSpanMeters <=
          camera.spec().maxVerticalSpanMeters + 1e-9);

    input.zoomSteps = -100.0;
    camera.apply(input);
    CHECK(camera.config().verticalSpanMeters >=
          camera.spec().minVerticalSpanMeters - 1e-9);
}

void testCameraIgnoresInputOnFocusLoss() {
    CameraController camera(testCameraConfig(), testCityBounds(), {});
    FrameInput input;
    input.dtSeconds = 1.0;
    input.viewport = {.width = 800, .height = 600};
    input.panRight = 1.0;
    input.focusLost = true;
    camera.apply(input);
    CHECK(camera.config().targetMeters.x == 0.0);
}

// --- snapshot fixtures --------------------------------------------------

struct SnapshotFixture {
    konbini::sim::FirstPlayableContent content;
    konbini::sim::GameState state;
    konbini::sim::FacilityTable facilities;
    konbini::sim::StoreTable stores;
    konbini::sim::PopulationCellTable populationCells;
    konbini::sim::WorldEntityIds identities;
};

konbini::sim::Bounds3 boundsAt(const double x, const double z) {
    return {
        .min = {.x = x - 5.0, .y = 0.0, .z = z - 5.0},
        .max = {.x = x + 5.0, .y = 10.0, .z = z + 5.0},
    };
}

SnapshotFixture makeFixture() {
    SnapshotFixture fixture;
    fixture.content =
        konbini::sim::loadFirstPlayableContent(KONBINI_TEST_CONTENT_FILE);
    fixture.state.phase = konbini::sim::GamePhase::Phase1;
    fixture.state.playerChain = konbini::sim::ChainId::Losan;

    for (std::uint32_t index = 0; index < 2; ++index) {
        const konbini::sim::FacilityId id =
            fixture.identities.facilities().acquire();
        const double x = 10.0 * static_cast<double>(index);
        fixture.facilities.append({
            .id = id,
            .figmentumKey = {.rawValue = 100 + index},
            .dimension = 0,
            .positionMeters = {.x = x, .y = 0.0, .z = 0.0},
            .boundsMeters = boundsAt(x, 0.0),
            // index 0 だけを配置候補にする。
            .isBuildable = index == 0,
            .state = konbini::sim::FacilityState::Intact,
        });
    }
    konbini::sim::initializePopulationCells(
        fixture.facilities, fixture.content, fixture.state.worldSeed,
        fixture.populationCells, fixture.identities.populationCells());
    return fixture;
}

std::shared_ptr<const konbini::sim::RenderSnapshot> makeSnapshot(
    const SnapshotFixture& fixture) {
    const konbini::sim::ChainEconomyTable economy(fixture.content);
    return konbini::sim::makeRenderSnapshot(
        fixture.state, fixture.content, fixture.facilities, fixture.stores,
        fixture.populationCells, economy, {});
}

// --- selection ----------------------------------------------------------
// 「1 回目で選択、同じ有効候補を再 click で確定」という 2 段操作。

void testSelectionRequiresSecondClickToConfirm() {
    const SnapshotFixture fixture = makeFixture();
    const auto snapshot = makeSnapshot(fixture);
    const konbini::sim::RenderFacility buildable = snapshot->facilities()[0];

    SelectionController selection;
    const konbini::render::FacilityPick pick{
        .figmentumKey = buildable.figmentumKey,
        .facilityId = buildable.id,
        .distanceMeters = 1.0,
    };

    const SelectionOutcome first = selection.onPrimaryClick(pick, *snapshot);
    CHECK(first.selected.has_value());
    CHECK(!first.placementRequested);

    const SelectionOutcome second = selection.onPrimaryClick(pick, *snapshot);
    CHECK(second.placementRequested);
    CHECK(second.selected.has_value());
    CHECK(*second.selected == buildable.id);
}

void testSelectionNeverConfirmsNonCandidates() {
    const SnapshotFixture fixture = makeFixture();
    const auto snapshot = makeSnapshot(fixture);
    const konbini::sim::RenderFacility blocked = snapshot->facilities()[1];
    CHECK(!SelectionController::isPlacementCandidate(blocked));

    SelectionController selection;
    const konbini::render::FacilityPick pick{
        .figmentumKey = blocked.figmentumKey,
        .facilityId = blocked.id,
        .distanceMeters = 1.0,
    };
    (void)selection.onPrimaryClick(pick, *snapshot);
    const SelectionOutcome second = selection.onPrimaryClick(pick, *snapshot);
    CHECK(!second.placementRequested);
}

void testSelectionClearsOnEmptyClickAndCancel() {
    const SnapshotFixture fixture = makeFixture();
    const auto snapshot = makeSnapshot(fixture);
    const konbini::sim::RenderFacility buildable = snapshot->facilities()[0];

    SelectionController selection;
    (void)selection.onPrimaryClick(
        konbini::render::FacilityPick{
            .figmentumKey = buildable.figmentumKey,
            .facilityId = buildable.id,
            .distanceMeters = 1.0},
        *snapshot);
    CHECK(selection.selected().has_value());

    const SelectionOutcome empty =
        selection.onPrimaryClick(std::nullopt, *snapshot);
    CHECK(!empty.selected.has_value());
    CHECK(!selection.selected().has_value());

    (void)selection.onPrimaryClick(
        konbini::render::FacilityPick{
            .figmentumKey = buildable.figmentumKey,
            .facilityId = buildable.id,
            .distanceMeters = 1.0},
        *snapshot);
    selection.clear();
    CHECK(!selection.selected().has_value());
}

void testSelectionIsDroppedWhenFacilityStopsBeingBuildable() {
    SnapshotFixture fixture = makeFixture();
    const auto snapshot = makeSnapshot(fixture);
    const konbini::sim::RenderFacility buildable = snapshot->facilities()[0];

    SelectionController selection;
    (void)selection.onPrimaryClick(
        konbini::render::FacilityPick{
            .figmentumKey = buildable.figmentumKey,
            .facilityId = buildable.id,
            .distanceMeters = 1.0},
        *snapshot);
    CHECK(selection.selected().has_value());

    // 店舗へ置換された後の snapshot では選択を保持しない。
    CHECK(fixture.facilities.setState(
        buildable.id, konbini::sim::FacilityState::Replaced));
    const auto replaced = makeSnapshot(fixture);
    selection.reconcile(*replaced);
    CHECK(!selection.selected().has_value());
}

// --- commands -----------------------------------------------------------

void testCommandOrderIsMonotonic() {
    CommandComposer composer;
    const konbini::sim::PlayerCommand first =
        composer.selectChain(konbini::sim::ChainId::Famoma, 3);
    const konbini::sim::PlayerCommand second =
        composer.selectChain(konbini::sim::ChainId::Famoma, 3);
    CHECK(konbini::sim::commandLess(first, second));
    CHECK(konbini::sim::commandOrder(first).targetTick == 3);
    CHECK(
        konbini::sim::commandOrder(second).sourcePriority ==
        konbini::sim::CommandSourcePriority::Player);
    CHECK(composer.issuedCount() == 2);
}

void testPlaceStoreRejectsDeadFacilityHandles() {
    CommandComposer composer;
    bool threw = false;
    try {
        (void)composer.placeStore(
            konbini::sim::ChainId::Losan, konbini::sim::FacilityId{}, 0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
}

// --- HUD ----------------------------------------------------------------

void testHudLinesAreRenderable() {
    HudTextInput input;
    input.hud.completedTicks = 42;
    input.hud.playerChain = konbini::sim::ChainId::SebanIleban;
    input.hud.cashCredits = 4000;
    input.hud.storeCount = 2;
    input.hud.ticksUntilEconomy = 3;
    input.hud.predictedEconomyIncomeCredits = 350;
    input.lastPlacementFailure = konbini::sim::PlacementFailure::InsufficientCash;
    input.droppedTicks = 2;

    const std::vector<std::string> lines =
        konbini::app::buildHudLines(input);
    CHECK(!lines.empty());
    for (const std::string& line : lines) {
        for (const char character : line) {
            CHECK(konbini::render::isBitmapFontCharacter(character));
        }
    }

    bool sawChain = false;
    bool sawFailure = false;
    bool sawDropped = false;
    for (const std::string& line : lines) {
        sawChain = sawChain || line == "CHAIN DAYLARK";
        sawFailure =
            sawFailure ||
            line == "PLACEMENT REJECTED - NOT ENOUGH CASH";
        sawDropped = sawDropped || line == "DROPPED TICKS 2";
    }
    CHECK(sawChain);
    CHECK(sawFailure);
    CHECK(sawDropped);
}

// 表示名は presentation 側の契約。3 chain すべてを固定し、bitmap font が
// 描けない文字が入り込まないことも確認する。
void testHudChainLabelsCoverEveryChain() {
    const std::pair<konbini::sim::ChainId, std::string> expected[] = {
        {konbini::sim::ChainId::Losan, "MOONPANTRY"},
        {konbini::sim::ChainId::Famoma, "SUNFOLD"},
        {konbini::sim::ChainId::SebanIleban, "DAYLARK"},
    };
    for (const auto& [chain, label] : expected) {
        const std::string actual = konbini::app::hudChainLabel(chain);
        CHECK(actual == label);
        for (const char character : actual) {
            CHECK(konbini::render::isBitmapFontCharacter(character));
        }
    }
}

void testHudHidesControlsWhenToggledOff() {
    HudTextInput input;
    input.showControls = false;
    const std::vector<std::string> lines =
        konbini::app::buildHudLines(input);
    for (const std::string& line : lines) {
        CHECK(line != "F1 TOGGLE THIS HELP");
    }
}

// --- HUD geometry -------------------------------------------------------

void testHudGeometryIsPixelSpaceAndBounded() {
    const std::vector<std::string> lines{"CASH 100", "TICK 7"};
    const konbini::render::HudTextStyle style =
        konbini::render::defaultHudTextStyle();
    const konbini::render::ViewportExtent extent{
        .width = 1280, .height = 720};
    const konbini::render::WorldMesh mesh = konbini::render::buildHudTextMesh(
        std::span<const std::string>(lines), style, extent);

    CHECK(!mesh.vertices.empty());
    CHECK(mesh.indices.size() % 6 == 0);
    for (const konbini::render::WorldVertex& vertex : mesh.vertices) {
        CHECK(vertex.position[0] >= -style.panelPaddingPixels);
        CHECK(vertex.position[1] >= -style.panelPaddingPixels);
        CHECK(vertex.position[0] <= static_cast<float>(extent.width));
        CHECK(vertex.position[1] <= static_cast<float>(extent.height));
        CHECK(vertex.position[2] == 0.0F);
    }
    for (const std::uint32_t index : mesh.indices) {
        CHECK(index < mesh.vertices.size());
    }
}

void testHudGeometryRejectsUnsupportedText() {
    const std::vector<std::string> lines{"cash"};
    bool threw = false;
    try {
        (void)konbini::render::buildHudTextMesh(
            std::span<const std::string>(lines),
            konbini::render::defaultHudTextStyle(),
            konbini::render::ViewportExtent{.width = 640, .height = 480});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
}

void testBitmapFontCoversHudRepertoire() {
    for (char character = '0'; character <= '9'; ++character) {
        CHECK(konbini::render::isBitmapFontCharacter(character));
        CHECK(konbini::render::bitmapGlyph5x7(character)[0] != 0 ||
              konbini::render::bitmapGlyph5x7(character)[3] != 0);
    }
    CHECK(konbini::render::isBitmapFontCharacter('-'));
    CHECK(konbini::render::isBitmapFontCharacter(':'));
    CHECK(konbini::render::isBitmapFontCharacter('/'));
    CHECK(konbini::render::isBitmapFontCharacter('.'));
    CHECK(konbini::render::isBitmapFontCharacter('+'));
    CHECK(!konbini::render::isBitmapFontCharacter('#'));
}

}  // namespace

// @implements spec/test/verification-strategy.md 1. Data / ID unit tests
// @spec 1. Data / ID unit tests
int main() {
    testFixedStepProducesWholeTicks();
    testFixedStepCapsCatchUpAndReportsDroppedTicks();
    testFixedStepRejectsInvalidConfiguration();
    testCameraPanUsesAzimuthBasis();
    testCameraClampsTargetToCityBounds();
    testCameraZoomIsClamped();
    testCameraIgnoresInputOnFocusLoss();
    testSelectionRequiresSecondClickToConfirm();
    testSelectionNeverConfirmsNonCandidates();
    testSelectionClearsOnEmptyClickAndCancel();
    testSelectionIsDroppedWhenFacilityStopsBeingBuildable();
    testCommandOrderIsMonotonic();
    testPlaceStoreRejectsDeadFacilityHandles();
    testHudLinesAreRenderable();
    testHudChainLabelsCoverEveryChain();
    testHudHidesControlsWhenToggledOff();
    testHudGeometryIsPixelSpaceAndBounded();
    testHudGeometryRejectsUnsupportedText();
    testBitmapFontCoversHudRepertoire();

    if (g_failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failures);
        return EXIT_FAILURE;
    }
    std::fprintf(stdout, "all checks passed\n");
    return EXIT_SUCCESS;
}
