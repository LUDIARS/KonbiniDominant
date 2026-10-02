#include <algorithm>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

#include "konbini/app/campaign_input_controller.h"
#include "konbini/app/command_composer.h"
#include "konbini/app/hud_layout_metrics.h"
#include "konbini/app/platform/display_metrics_channel.h"
#include "konbini/app/pointer_controls.h"
#include "konbini/app/pointer_hud_text.h"
#include "konbini/app/pointer_input_controller.h"
#include "konbini/app/selection_controller.h"
#include "konbini/app/selection_hud.h"
#include "konbini/app/selection_preview_text.h"
#include "konbini/app/simulation_host.h"
#include "konbini/app/touch_contacts.h"
#include "konbini/city/i_city_generator.h"

#include "../check.h"

// KD-MOB-004 touch placement flow and the responsive HUD.
// @implements spec/feature/ui-ux.md Smartphone interaction
// @implements spec/test/verification-strategy.md 1. Data / ID unit tests

namespace {

using namespace konbini;
using namespace konbini::app;

// Phase 1 content builds the grid town itself; the Figmentum path is never used.
class UnusedCityGenerator final : public city::ICityGenerator {
public:
    city::CityManifest planFirstPlayableCity(sim::GenerationalIdPool<sim::FacilityId>&) const override {
        throw std::logic_error("grid town does not plan through Figmentum");
    }
    std::shared_ptr<const city::FacilityGeometry> buildFacility(const city::ManifestFacility&) const override {
        throw std::logic_error("grid town does not mesh through Figmentum");
    }
    city::GeneratedCity generateFirstPlayableCity(sim::GenerationalIdPool<sim::FacilityId>&) const override {
        throw std::logic_error("grid town does not generate through Figmentum");
    }
};

// A Phase 1 match after the player chose a chain.
struct PlayingMatch {
    UnusedCityGenerator generator;
    SimulationHost host{KONBINI_PHASE1_CONTENT_FILE, generator};
    CommandComposer commands;
    PlayingMatch() {
        host.submit(commands.selectChain(sim::ChainId::Losan, host.completedTicks()));
        (void)host.tick();
        if (host.snapshot()->hud().phase != sim::GamePhase::Phase1)
            throw std::logic_error("phase 1 did not start");
    }
    [[nodiscard]] sim::FacilityId firstCandidate() const {
        for (const auto& facility : host.snapshot()->facilities())
            if (SelectionController::isPlacementCandidate(facility)) return facility.id;
        throw std::logic_error("no placement candidate");
    }
};

[[nodiscard]] FrameInput touchFrame(const PointerSample& sample, render::ViewportExtent viewport) {
    FrameInput input;
    input.pointer = sample;
    input.viewport = viewport;
    input.cursorXPixels = sample.xPixels;
    input.cursorYPixels = sample.yPixels;
    input.cursorInsideViewport = true;
    return input;
}

[[nodiscard]] const PointerButton* findButton(const PointerControls& controls, PointerAction action) {
    for (const auto& button : controls.buttons)
        if (button.action == action) return &button;
    return nullptr;
}

// C-7: tap selects, the explicit Place action confirms.
void tapSelectsAndPlaceConfirms() {
    CHECK(tapPlacementFor(true, true) == TapPlacement::SelectOnly);
    CHECK(tapPlacementFor(true, false) == TapPlacement::SelectOnly);
    CHECK(tapPlacementFor(false, true) == TapPlacement::PlaceImmediately);
    CHECK(tapPlacementFor(false, false) == TapPlacement::ConfirmOnRepeat);

    PlayingMatch match;
    const auto snapshot = match.host.snapshot();
    const auto lot = match.firstCandidate();
    SelectionController selection;
    const render::FacilityPick pick{.facilityId = lot};
    const auto first = selection.onPrimaryClick(pick, *snapshot, TapPlacement::SelectOnly);
    CHECK(first.selected == lot && !first.placementRequested);
    // Tapping the same lot again still never buys it.
    const auto again = selection.onPrimaryClick(pick, *snapshot, TapPlacement::SelectOnly);
    CHECK(again.selected == lot && !again.placementRequested);

    auto hud = makeSelectionHud(*snapshot, match.host.content(), selection.selected(), 0);
    hud.gridPlacement = true;
    hud.tapSelectsOnly = true;
    const render::ViewportExtent viewport{1280, 720};
    const auto controls = buildPointerControls(hud, HudLayoutMetrics{.extentPixels = viewport}, PointerPage::Main);
    const auto* place = findButton(controls, PointerAction::Place);
    CHECK(place != nullptr && place->label == "PLACE" && place->enabled);

    TouchContacts contacts;
    PointerInputController controller;
    const double x = place->rect.x + place->rect.width / 2, y = place->rect.y + place->rect.height / 2;
    contacts.update(TouchSample{1, TouchPhase::Down, x, y, 0.0});
    contacts.update(TouchSample{1, TouchPhase::Up, x, y, 0.1});
    const auto input = controller.translate(touchFrame(contacts.consume(), viewport), controls, 0);
    CHECK(input.buildRequested && !input.primaryClick);

    CampaignInputController campaign;
    const auto commands = campaign.actions(input, *snapshot, selection.selected(),
                                           match.host.completedTicks(), match.commands);
    CHECK(commands.size() == 1);
    CHECK(!commands.empty() && std::holds_alternative<sim::PlaceStoreCommand>(commands.front()));
    if (!commands.empty() && std::holds_alternative<sim::PlaceStoreCommand>(commands.front()))
        CHECK(std::get<sim::PlaceStoreCommand>(commands.front()).facilityId == lot);
}

// C-8: hover information comes from the selection state, not the pointer.
void selectionStateReplacesHover() {
    PlayingMatch match;
    const auto snapshot = match.host.snapshot();
    const auto lot = match.firstCandidate();
    auto hud = makeSelectionHud(*snapshot, match.host.content(), lot, 0);
    hud.gridPlacement = true;
    hud.tapSelectsOnly = true;
    const auto preview = buildSelectionPreviewLines(hud);
    CHECK(preview.size() == 1);
    CHECK(!preview.empty() && preview.front() ==
          "SELECTED LOT COST " + std::to_string(hud.selectedBuildCostCredits));
    CHECK(placementHintLine(hud) == "TAP PLACE TO BUILD A STORE");
    const auto lines = buildPointerHudLines(hud);
    CHECK(std::find(lines.begin(), lines.end(), preview.front()) != lines.end());

    auto unselected = makeSelectionHud(*snapshot, match.host.content(), std::nullopt, 0);
    unselected.gridPlacement = true;
    CHECK(buildSelectionPreviewLines(unselected).empty());
    CHECK(placementHintLine(unselected) == "TAP EMPTY GRID TO BUILD");
    unselected.tapSelectsOnly = true;
    CHECK(placementHintLine(unselected) == "TAP A LOT TO SELECT");
}

[[nodiscard]] bool insideSafeRect(const PointerRect& rect, const HudSafeRect& safe) {
    return rect.x >= safe.x && rect.y >= safe.y &&
           rect.x + rect.width <= safe.x + safe.width + 0.01F &&
           rect.y + rect.height <= safe.y + safe.height + 0.01F;
}

[[nodiscard]] DisplayMetrics landscapePhone() {
    return {.extentPixels = {2400, 1080}, .density = 3.0, .safeArea = {96, 0, 48, 60}};
}
[[nodiscard]] DisplayMetrics portraitPhone() {
    return {.extentPixels = {1080, 2400}, .density = 3.0, .safeArea = {0, 96, 0, 48}};
}

// C-9: safe area, density and UI scale drive the HUD layout.
void hudFollowsSafeAreaDensityAndScale() {
    HudTextInput hud;
    hud.hud.phase = sim::GamePhase::Phase1;
    hud.hud.playerChain = sim::ChainId::Losan;
    for (const auto& display : {landscapePhone(), portraitPhone()}) {
        const auto metrics = hudLayoutFromDisplay(display);
        const auto safe = hudSafeRect(metrics);
        const auto controls = buildPointerControls(hud, metrics, PointerPage::Main);
        for (const auto& button : controls.buttons) CHECK(insideSafeRect(button.rect, safe));
        CHECK(controls.statusStyle.originXPixels >= safe.x);
        CHECK(controls.statusStyle.originYPixels >= safe.y);
        // The capture panel reaches the bottom edge, inset included.
        CHECK(controls.panel.y + controls.panel.height == static_cast<float>(display.extentPixels.height));
    }
    const render::ViewportExtent tall{1080, 2400};
    const auto lowDensity = buildPointerControls(hud, HudLayoutMetrics{.extentPixels = tall, .density = 0.75}, PointerPage::Main);
    const auto highDensity = buildPointerControls(hud, HudLayoutMetrics{.extentPixels = tall, .density = 1.25}, PointerPage::Main);
    const auto enlarged = buildPointerControls(hud, HudLayoutMetrics{.extentPixels = tall, .density = 1.0, .userUiScale = 1.5}, PointerPage::Main);
    CHECK(lowDensity.uiScale < highDensity.uiScale);
    CHECK(enlarged.uiScale > buildPointerControls(hud, HudLayoutMetrics{.extentPixels = tall}, PointerPage::Main).uiScale);
    CHECK(highDensity.buttons.front().rect.height > lowDensity.buttons.front().rect.height);
    CHECK_THROWS(std::invalid_argument, (void)hudLayoutFromDisplay(landscapePhone(), 3.0));
    CHECK_THROWS(std::invalid_argument, (void)hudLayoutFromDisplay(DisplayMetrics{}));
}

// C-10: rotation / resize keeps selection and simulation state.
void rotationKeepsSelectionAndSimulation() {
    PlayingMatch match;
    const auto lot = match.firstCandidate();
    SelectionController selection;
    (void)selection.onPrimaryClick(render::FacilityPick{.facilityId = lot}, *match.host.snapshot(),
                                   TapPlacement::SelectOnly);
    const auto snapshotBefore = match.host.snapshot();
    const auto ticksBefore = match.host.completedTicks();

    DisplayMetricsChannel channel;
    std::optional<HudLayoutMetrics> layout;
    int rotations = 0;
    (void)channel.subscribe([&](const DisplayMetricsChange& change) {
        layout = hudLayoutFromDisplay(change.current);
        rotations += change.orientationChanged() ? 1 : 0;
    });
    CHECK(channel.publish(landscapePhone()));

    PointerInputController controller;
    TouchContacts contacts;
    auto hud = makeSelectionHud(*match.host.snapshot(), match.host.content(), selection.selected(), 0);
    hud.tapSelectsOnly = true;
    const auto landscapeControls = buildPointerControls(hud, *layout, PointerPage::Main);
    contacts.update(TouchSample{1, TouchPhase::Down, 1200, 400, 0.0});
    (void)controller.translate(touchFrame(contacts.consume(), layout->extentPixels), landscapeControls, 0);
    CHECK(controller.gestureActive());

    CHECK(channel.publish(portraitPhone()));
    // The first publish counts as an orientation change (no previous layout).
    CHECK(rotations == 2);
    const auto portraitControls = buildPointerControls(hud, *layout, PointerPage::Main);
    const auto afterRotation = controller.translate(touchFrame(contacts.consume(), layout->extentPixels), portraitControls, 0);
    // The in-flight gesture is dropped, nothing else is.
    CHECK(!controller.gestureActive() && !afterRotation.primaryClick);
    CHECK(controller.lastCancel() == GestureCancelReason::SurfaceChanged);
    CHECK(selection.selected() == lot);
    CHECK(match.host.completedTicks() == ticksBefore);
    CHECK(match.host.snapshot() == snapshotBefore);
    const auto* place = findButton(portraitControls, PointerAction::Place);
    CHECK(place != nullptr && place->enabled);
    CHECK(buildSelectionPreviewLines(hud) ==
          buildSelectionPreviewLines(makeSelectionHud(*match.host.snapshot(), match.host.content(), selection.selected(), 0)));
}

}  // namespace

int main() {
    CHECK_NO_THROW(tapSelectsAndPlaceConfirms());
    CHECK_NO_THROW(selectionStateReplacesHover());
    CHECK_NO_THROW(hudFollowsSafeAreaDensityAndScale());
    CHECK_NO_THROW(rotationKeepsSelectionAndSimulation());
    return konbini::test::summarize("touch HUD layout");
}
