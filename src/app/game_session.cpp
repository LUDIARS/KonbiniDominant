#include "konbini/app/game_session.h"

#include <cstdio>
#include <algorithm>
#include <cmath>
#include "konbini/app/playtest_pilot.h"
#include "konbini/app/playtest_report.h"
#include <optional>
#include <stdexcept>
#include <utility>

#include "konbini/adapters/ergo/world_frame_graph.h"
#include "konbini/adapters/figmentum/figmentum_city_adapter.h"
#include "konbini/adapters/pictor/world_geometry_loader.h"
#include "konbini/app/camera_controller.h"
#include "konbini/app/campaign_input_controller.h"
#include "konbini/render/campaign_floor_view.h"
#include "konbini/sim/vertical_placement.h"
#include "konbini/sim/skill_system.h"
#include "konbini/app/pointer_input_controller.h"
#include "konbini/app/selection_hud.h"
#include "konbini/app/command_composer.h"
#include "konbini/app/fixed_step_driver.h"
#include "konbini/app/frame_presenter.h"
#include "konbini/app/hud_text_model.h"
#include "konbini/app/selection_controller.h"
#include "konbini/app/simulation_host.h"
#include "konbini/render/facility_picker.h"
#include "konbini/render/grid_ground.h"
#include "konbini/city/grid_town.h"
#include "konbini/render/isometric_camera.h"

namespace konbini::app {
namespace {
[[nodiscard]] render::IsometricCameraConfig makeInitialCamera(
    const city::CityManifest& manifest) {
    const double spanX = manifest.boundsMeters.max.x - manifest.boundsMeters.min.x;
    const double spanZ = manifest.boundsMeters.max.z - manifest.boundsMeters.min.z;
    render::IsometricCameraConfig config;
    config.targetMeters = manifest.stationAnchorMeters;
    // 縦方向の可視範囲は区画全体が入る大きさにする。斜め見下ろしなので
    // 対角相当の余裕を取る。
    config.verticalSpanMeters =
        1.4 * (spanX > spanZ ? spanX : spanZ) + 20.0;
    if (city::isGridTown(manifest)) config.verticalSpanMeters = 150.0;
    config.distanceMeters = config.verticalSpanMeters * 2.0 + 100.0;
    config.farPlaneMeters = config.distanceMeters * 4.0;
    return config;
}


}  // namespace
namespace {
render::WorldDrawListSpec drawSpecFor(const city::CityManifest& manifest) {
    auto spec = render::defaultWorldDrawListSpec();
    spec.gridTown = city::isGridTown(manifest);
    if (spec.gridTown) {
        spec.storeMarker.gridCellMeters = city::kGridCellMeters;
        spec.storeMarker.halfWidthMeters = city::kGridCellMeters * 0.5;
        spec.storeMarker.heightMeters = 2.8;
    }
    return spec;
}
}
struct GameSession::Impl {
    Impl(const std::filesystem::path& contentFile,const std::uint32_t maxTicks,PlaytestOptions options)
        : playtest(std::move(options)),report(playtest),simulation(contentFile,cityGenerator),
          camera(makeInitialCamera(simulation.city().manifest),simulation.city().manifest.boundsMeters,{}),
          fixedStep(simulation.content().simulation.ticksPerSecond,std::max(maxTicks,static_cast<std::uint32_t>(
              std::ceil(simulation.content().simulation.ticksPerSecond*playtest.timeScale*kMaxRenderDeltaSeconds)))),
          presenter(drawSpecFor(simulation.city().manifest),render::defaultHudTextStyle()) {}
    PlaytestOptions playtest;
    PlaytestReport report;
    PlaytestPilot pilot;
    adapters::figmentum::FigmentumCityAdapter cityGenerator;
    SimulationHost simulation;
    CameraController camera;
    FixedStepDriver fixedStep;
    FramePresenter presenter;
    SelectionController selection;
    CommandComposer commands;
    CampaignInputController campaignInput;
    PointerInputController pointerInput;
    bool showControls = false;
    bool isPaused = false;
    std::optional<sim::PlacementFailure> lastPlacementFailure;
    std::optional<sim::SelectChainFailure> lastChainFailure;
    std::optional<sim::CampaignFailure> lastCampaignFailure;
    std::uint32_t lastDroppedTicks = 0;
};
GameSession::GameSession(const std::filesystem::path& file,const std::uint32_t maxTicks,PlaytestOptions playtest)
    : impl_(std::make_unique<Impl>(file,maxTicks,std::move(playtest))) {}
GameSession::~GameSession()=default;
void GameSession::notifyPresented() {impl_->report.presented(*impl_->simulation.snapshot());}
void GameSession::suspend() noexcept {
    impl_->fixedStep.drain();
    impl_->pointerInput.cancelGesture();
}
void GameSession::uploadGeometry(adapters::ergo::WorldFrameGraph& graph) {
    const auto report=adapters::pictor::loadCityGeometry(impl_->simulation.city(),graph.geometryCache());
    std::fprintf(stdout,"[konbini] uploaded %zu facility meshes (%zu shared)\n",report.uploaded,report.deduplicated);
}
void GameSession::frame(FrameInput input,const render::ViewportExtent extent,adapters::ergo::WorldFrameGraph& graph) {
    const double deltaSeconds=input.dtSeconds;
    impl_->presenter.advance(impl_->isPaused ? 0.0 : deltaSeconds);
    auto snapshot=impl_->simulation.snapshot();
    auto beforeInput=makeSelectionHud(*snapshot,impl_->simulation.content(),impl_->selection.selected(),impl_->campaignInput.floor());
    beforeInput.gridPlacement=city::isGridTown(impl_->simulation.city().manifest);
    beforeInput.isPaused=impl_->isPaused;beforeInput.showControls=impl_->showControls;
    beforeInput.lastPlacementFailure=impl_->lastPlacementFailure;
    beforeInput.lastChainFailure=impl_->lastChainFailure;
    beforeInput.lastCampaignFailure=impl_->lastCampaignFailure;
    beforeInput.droppedTicks=impl_->lastDroppedTicks;
    const auto controls=buildPointerControls(beforeInput,extent,impl_->pointerInput.page(),input.uiScale);
    const auto context=static_cast<std::uint64_t>(snapshot->hud().phase) |
        (static_cast<std::uint64_t>(snapshot->hud().campaign.skills.pending)<<8) |
        (static_cast<std::uint64_t>(snapshot->hud().campaign.skills.level)<<16) |
        (static_cast<std::uint64_t>(snapshot->hud().campaign.visibleDimension)<<32);
    input=impl_->pointerInput.translate(input,controls,context);

    if (input.toggleControls) {
        impl_->showControls = !impl_->showControls;
    }
    if (input.cancel) {
        impl_->selection.clear();
    }

    if (input.retry && snapshot->hud().phase == sim::GamePhase::Result) {
        impl_->simulation.retry();
        impl_->presenter.reset();
        impl_->pilot=PlaytestPilot{};
        impl_->report.restart();
        impl_->selection.clear();
        impl_->commands = CommandComposer{};
        impl_->campaignInput.reset();
        impl_->pointerInput.reset();
        impl_->lastCampaignFailure.reset();
        impl_->fixedStep.drain();
        impl_->lastPlacementFailure.reset();
        impl_->lastChainFailure.reset();
        impl_->isPaused = false;
        snapshot = impl_->simulation.snapshot();
    }
    if (input.togglePause && !snapshot->hud().campaign.skills.pending && sim::isPlayingPhase(snapshot->hud().phase)) {
        impl_->isPaused = !impl_->isPaused;
        impl_->fixedStep.drain();
    }
    impl_->campaignInput.updateFloor(input, *snapshot, impl_->selection.selected());
    impl_->camera.apply(input);
    impl_->camera.focusHeight(impl_->campaignInput.floor() *
        snapshot->hud().campaign.floorHeightMeters);
    render::IsometricCamera camera =
        render::buildIsometricCamera(impl_->camera.config(), extent);
    const std::uint64_t targetTick = impl_->simulation.completedTicks();

    if (input.chainRequest.has_value() && !impl_->isPaused &&
        snapshot->hud().phase == sim::GamePhase::ChainSelect) {
        impl_->simulation.submit(
            impl_->commands.selectChain(*input.chainRequest, targetTick));
    }

    if (input.primaryClick && !impl_->isPaused && !snapshot->hud().campaign.skills.pending &&
        sim::isPlayingPhase(snapshot->hud().phase)) {
        const render::WorldRay ray = render::makeWorldRay(
            camera, input.cursorXPixels, input.cursorYPixels, extent);
        const bool gridTown = city::isGridTown(impl_->simulation.city().manifest);
        const auto pick = gridTown
            ? render::pickGridCell(ray, snapshot->facilities(),
                impl_->campaignInput.floor() * snapshot->hud().campaign.floorHeightMeters)
            : snapshot->hud().campaign.enabled && snapshot->hud().campaign.reachedPhase >= 2
                ? render::pickCampaignFloor(ray, *snapshot, impl_->campaignInput.floor())
                : render::pickFacility(ray, snapshot->facilities());
        const SelectionOutcome outcome = impl_->selection.onPrimaryClick(pick, *snapshot,
            gridTown && snapshot->hud().phase == sim::GamePhase::Phase1);
        if (outcome.placementRequested &&
            snapshot->hud().playerChain.has_value() &&
            outcome.selected.has_value()) {
            impl_->simulation.submit(impl_->commands.placeStore(
                *snapshot->hud().playerChain, *outcome.selected,
                targetTick, impl_->campaignInput.floor()));
        }
    }

    if (!impl_->isPaused) {
        for (const auto& command : impl_->campaignInput.actions(
                 input, *snapshot, impl_->selection.selected(), targetTick,
                 impl_->commands)) {
            impl_->simulation.submit(command);
        }
    }

    std::optional<std::uint32_t> skillChoice=input.skillChoice;
    if(snapshot->hud().campaign.skills.pending && !impl_->isPaused) {
        if(input.chainRequest) skillChoice=static_cast<std::uint32_t>(sim::chainIndex(*input.chainRequest));
        if(skillChoice) impl_->simulation.submit(impl_->commands.campaignAction(
            *snapshot->hud().playerChain,sim::CampaignAction::ChooseSkill,{},0,*skillChoice,targetTick));
    }
    FixedStepPlan plan;
    if(snapshot->hud().campaign.skills.pending) {
        impl_->fixedStep.drain();
        plan.tickCount=(skillChoice || impl_->playtest.autoplay) ? 1 : 0;
    } else if (impl_->isPaused || snapshot->hud().phase == sim::GamePhase::Result) {
        impl_->fixedStep.drain();
    } else {
        plan = impl_->fixedStep.advance(deltaSeconds,impl_->playtest.timeScale);
    }
    impl_->lastDroppedTicks = plan.droppedTicks;
    if (plan.droppedTicks != 0) {
        std::fprintf(
            stderr, "[konbini] dropped %u simulation tick(s)\n",
            plan.droppedTicks);
    }
    for (std::uint32_t step = 0; step < plan.tickCount; ++step) {
        if(impl_->playtest.autoplay) {
            const auto command=impl_->pilot.next(*impl_->simulation.snapshot(),impl_->commands);
            if(command) impl_->simulation.submit(*command);
        }
        const sim::CompletedTick completed = impl_->simulation.tick();
        impl_->presenter.observe(*completed.render);
        impl_->report.observe(*completed.render,impl_->playtest.autoplay?impl_->pilot.activeNode():"manual");
        bool advanceBuiltFloor=false;
        for (const sim::SelectChainResult& result :
             completed.chainSelections) {
            impl_->lastChainFailure = result.failure;
        }
        for (const sim::PlacementResult& result : completed.placements) {
            if (result.command.order.sourcePriority == sim::CommandSourcePriority::Player) {
                impl_->lastPlacementFailure = result.failure;
                advanceBuiltFloor=advanceBuiltFloor || (result.failure==sim::PlacementFailure::None &&
                    impl_->selection.selected()==result.command.facilityId &&
                    impl_->campaignInput.floor()==result.command.verticalSlot);
            }
        }
        for (const auto& result : completed.campaignActions) {
            impl_->lastCampaignFailure = result.failure;
        }
        snapshot = completed.render;
        if(advanceBuiltFloor) {
            FrameInput nextFloor;nextFloor.nextFreeFloor=true;
            impl_->campaignInput.updateFloor(nextFloor,*snapshot,impl_->selection.selected());
        }
        if(snapshot->hud().campaign.skills.pending) {
            impl_->fixedStep.drain();
            break;
        }
        if (snapshot->hud().phase == sim::GamePhase::Result) {
            impl_->selection.clear();
            impl_->fixedStep.drain();
            break;
        }
    }
    if (plan.tickCount != 0) {
        impl_->selection.reconcile(*snapshot);
    }

    // A build can advance the viewed floor during this frame.
    impl_->camera.focusHeight(impl_->campaignInput.floor() *
        snapshot->hud().campaign.floorHeightMeters);
    camera = render::buildIsometricCamera(impl_->camera.config(), extent);

    HudTextInput hudInput=makeSelectionHud(*snapshot,impl_->simulation.content(),impl_->selection.selected(),impl_->campaignInput.floor());
    hudInput.gridPlacement=city::isGridTown(impl_->simulation.city().manifest);
    hudInput.hud = snapshot->hud();
    hudInput.selectedFloor = impl_->campaignInput.floor();
    hudInput.lastCampaignFailure = impl_->lastCampaignFailure;
    hudInput.selectedFacility = impl_->selection.selected();
    hudInput.lastPlacementFailure = impl_->lastPlacementFailure;
    hudInput.lastChainFailure = impl_->lastChainFailure;
    hudInput.showControls = impl_->showControls;
    hudInput.droppedTicks = impl_->lastDroppedTicks;
    hudInput.isPaused = impl_->isPaused;

    impl_->presenter.present(
        graph.worldLayer(), graph.hudLayer(), *snapshot,
        camera, impl_->selection.selected(), hudInput,
        buildPointerControls(hudInput,extent,impl_->pointerInput.page(),input.uiScale),impl_->pointerInput.pressed());

}
}  // namespace konbini::app
