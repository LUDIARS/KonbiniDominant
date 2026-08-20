#include "konbini/app/app_runner.h"

#include <cstdio>
#include <optional>
#include <stdexcept>
#include <utility>

#include "ergo/frame/frame.h"
#include "ergo/input/input_system.h"
#include "konbini/adapters/ergo/ergo_input_bridge.h"
#include "konbini/adapters/ergo/input_action_map.h"
#include "konbini/adapters/ergo/render_device_host.h"
#include "konbini/adapters/ergo/swapchain_identity.h"
#include "konbini/adapters/ergo/world_frame_graph.h"
#include "konbini/adapters/figmentum/figmentum_city_adapter.h"
#include "konbini/adapters/pictor/world_geometry_loader.h"
#include "konbini/app/camera_controller.h"
#include "konbini/app/command_composer.h"
#include "konbini/app/fixed_step_driver.h"
#include "konbini/app/frame_presenter.h"
#include "konbini/app/hud_text_model.h"
#include "konbini/app/selection_controller.h"
#include "konbini/app/simulation_host.h"
#include "konbini/render/facility_picker.h"
#include "konbini/render/isometric_camera.h"

// @implements spec/plan/tasks/first-playable.md Build and process boundary
// @implements spec/interface/ergo-runtime.md Frame / simulation

namespace konbini::app {
namespace {

// 都市 bounds から初期 camera を組む。stationAnchor を見ることで、生成された
// 区画の中心が必ず画面に入る。
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
    config.distanceMeters = config.verticalSpanMeters * 2.0 + 100.0;
    config.farPlaneMeters = config.distanceMeters * 4.0;
    return config;
}

[[nodiscard]] const sim::RenderFacility* findFacility(
    const sim::RenderSnapshot& snapshot, const sim::FacilityId id) noexcept {
    for (const sim::RenderFacility& facility : snapshot.facilities()) {
        if (facility.id == id) {
            return &facility;
        }
    }
    return nullptr;
}

}  // namespace

struct AppRunner::Impl {
    explicit Impl(AppRunnerConfig configuration)
        : config(std::move(configuration)),
          simulation(config.paths.contentFile, cityGenerator),
          camera(
              makeInitialCamera(simulation.city().manifest),
              simulation.city().manifest.boundsMeters, {}),
          fixedStep(
              simulation.content().simulation.ticksPerSecond,
              config.maxTicksPerFrame),
          presenter(
              render::defaultWorldDrawListSpec(),
              render::defaultHudTextStyle()) {}

    AppRunnerConfig config;
    adapters::figmentum::FigmentumCityAdapter cityGenerator;
    SimulationHost simulation;
    CameraController camera;
    FixedStepDriver fixedStep;
    FramePresenter presenter;
    SelectionController selection;
    CommandComposer commands;
    adapters::ergo::RenderDeviceHost device;
    adapters::ergo::WorldFrameGraph graph;
    ::ergo::input::InputSystem input;
    adapters::ergo::ErgoInputBridge bridge;
    adapters::ergo::InputActionMap actionMap;
    bool inputInitialized = false;
    bool showControls = true;
    std::optional<sim::PlacementFailure> lastPlacementFailure;
    std::optional<sim::SelectChainFailure> lastChainFailure;
    std::uint32_t lastDroppedTicks = 0;
};

AppRunner::AppRunner(AppRunnerConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

AppRunner::~AppRunner() {
    // 逆順解放。input bridge -> frame graph -> device host。
    if (impl_ != nullptr) {
        impl_->bridge.detach();
        impl_->graph.shutdown();
        if (impl_->inputInitialized) {
            impl_->input.shutdown();
            impl_->inputInitialized = false;
        }
        impl_->device.shutdown();
    }
}

// @implements spec/plan/tasks/first-playable.md Input and rendering
int AppRunner::run() {
    validateAppPaths(impl_->config.paths);

    adapters::ergo::RenderDeviceConfig deviceConfig;
    deviceConfig.windowWidth = impl_->config.windowWidth;
    deviceConfig.windowHeight = impl_->config.windowHeight;
    deviceConfig.windowTitle = impl_->config.windowTitle;
    deviceConfig.framesInFlight = impl_->config.framesInFlight;
    deviceConfig.validation = impl_->config.validation;
    deviceConfig.shaderDirectory = impl_->config.paths.shaderDirectory.string();
    deviceConfig.assetRoot =
        impl_->config.paths.contentFile.parent_path().string();
    impl_->device.initialize(deviceConfig);

    impl_->graph.initialize(impl_->device);
    const adapters::pictor::WorldGeometryLoadReport geometryReport =
        adapters::pictor::loadCityGeometry(
            impl_->simulation.city(), impl_->graph.geometryCache());
    std::fprintf(
        stdout, "[konbini] uploaded %zu facility meshes (%zu shared)\n",
        geometryReport.uploaded, geometryReport.deduplicated);

    ::ergo::input::InputConfig inputConfig;
    inputConfig.threadMode = ::ergo::input::ThreadMode::MainSync;
    inputConfig.enableMouse = true;
    inputConfig.enableKeyboard = true;
    inputConfig.enableGamepad = false;
    inputConfig.enableUsb = false;
    impl_->input.initialize(inputConfig);
    impl_->inputInitialized = true;
    impl_->bridge.attach(impl_->device.window(), impl_->input);

    ::ergo::frame::reset();

    int exitCode = 0;
    while (!impl_->device.shouldClose()) {
        impl_->device.pollEvents();
        impl_->bridge.beginFrame();
        ::ergo::frame::tick();
        const double deltaSeconds =
            static_cast<double>(::ergo::frame::dt_seconds());

        const render::ViewportExtent windowExtent =
            impl_->device.framebufferExtent();
        if (windowExtent.width == 0 || windowExtent.height == 0) {
            // 最小化中は tick も GPU submission も進めない。溜まった時間を
            // 捨てて、復帰時に catch-up が張り付かないようにする。event を
            // 短時間待ち、restore / close 待ちで busy-loop しない。
            impl_->fixedStep.drain();
            impl_->bridge.endFrame();
            impl_->device.waitEvents(0.05);
            continue;
        }
        if (windowExtent != impl_->device.swapchainExtent()) {
            // Pictor が自発的に作り直すのは acquire / present の失敗時だけ
            // なので、resize は host から明示的に要求する。
            impl_->graph.requestRebuild();
        }
        if (impl_->graph.rebuildPending()) {
            // Rebuild before sampling input or publishing presentation state.
            // Otherwise this frame would pick against the old extent and the
            // rebuild would discard the world/HUD state just published.
            (void)impl_->graph.runFrame(0.0F);
            impl_->bridge.endFrame();
            continue;
        }

        const render::ViewportExtent extent = impl_->graph.extent();
        if (extent.width == 0 || extent.height == 0) {
            impl_->fixedStep.drain();
            impl_->bridge.endFrame();
            continue;
        }

        const FrameInput input = impl_->actionMap.sample(
            impl_->input, impl_->bridge, extent, deltaSeconds);

        impl_->camera.apply(input);
        const render::IsometricCamera camera =
            render::buildIsometricCamera(impl_->camera.config(), extent);

        if (input.toggleControls) {
            impl_->showControls = !impl_->showControls;
        }
        if (input.cancel) {
            impl_->selection.clear();
        }

        std::shared_ptr<const sim::RenderSnapshot> snapshot =
            impl_->simulation.snapshot();
        const std::uint64_t targetTick = impl_->simulation.completedTicks();

        if (input.chainRequest.has_value()) {
            impl_->simulation.submit(
                impl_->commands.selectChain(*input.chainRequest, targetTick));
        }

        if (input.primaryClick) {
            const render::WorldRay ray = render::makeWorldRay(
                camera, input.cursorXPixels, input.cursorYPixels, extent);
            const SelectionOutcome outcome = impl_->selection.onPrimaryClick(
                render::pickFacility(ray, snapshot->facilities()), *snapshot);
            if (outcome.placementRequested &&
                snapshot->hud().playerChain.has_value() &&
                outcome.selected.has_value()) {
                impl_->simulation.submit(impl_->commands.placeStore(
                    *snapshot->hud().playerChain, *outcome.selected,
                    targetTick));
            }
        }

        const FixedStepPlan plan = impl_->fixedStep.advance(deltaSeconds);
        impl_->lastDroppedTicks = plan.droppedTicks;
        if (plan.droppedTicks != 0) {
            std::fprintf(
                stderr, "[konbini] dropped %u simulation tick(s)\n",
                plan.droppedTicks);
        }
        for (std::uint32_t step = 0; step < plan.tickCount; ++step) {
            const sim::CompletedTick completed = impl_->simulation.tick();
            for (const sim::SelectChainResult& result :
                 completed.chainSelections) {
                impl_->lastChainFailure = result.failure;
            }
            for (const sim::PlacementResult& result : completed.placements) {
                impl_->lastPlacementFailure = result.failure;
            }
            snapshot = completed.render;
        }
        if (plan.tickCount != 0) {
            impl_->selection.reconcile(*snapshot);
        }

        HudTextInput hudInput;
        hudInput.hud = snapshot->hud();
        hudInput.selectedFacility = impl_->selection.selected();
        hudInput.lastPlacementFailure = impl_->lastPlacementFailure;
        hudInput.lastChainFailure = impl_->lastChainFailure;
        hudInput.showControls = impl_->showControls;
        hudInput.droppedTicks = impl_->lastDroppedTicks;
        if (hudInput.selectedFacility.has_value()) {
            const sim::RenderFacility* const facility =
                findFacility(*snapshot, *hudInput.selectedFacility);
            hudInput.selectionIsPlacementCandidate =
                facility != nullptr &&
                SelectionController::isPlacementCandidate(*facility);
            if (snapshot->hud().playerChain.has_value()) {
                hudInput.selectedBuildCostCredits =
                    impl_->simulation.content()
                        .chain(*snapshot->hud().playerChain)
                        .buildCostCredits;
            }
        }

        impl_->presenter.present(
            impl_->graph.worldLayer(), impl_->graph.hudLayer(), *snapshot,
            camera, impl_->selection.selected(), hudInput);

        const adapters::ergo::FrameOutcome outcome =
            impl_->graph.runFrame(static_cast<float>(deltaSeconds));
        if (adapters::ergo::isFatal(outcome)) {
            std::fprintf(
                stderr, "[konbini] fatal render state: %s\n",
                adapters::ergo::describeFrameOutcome(outcome));
            exitCode = 2;
            impl_->bridge.endFrame();
            break;
        }
        impl_->bridge.endFrame();
    }

    // 正常終了経路。逆順で解放し、destructor の二重解放を避ける。
    impl_->bridge.detach();
    impl_->graph.shutdown();
    impl_->input.shutdown();
    impl_->inputInitialized = false;
    impl_->device.shutdown();
    return exitCode;
}

}  // namespace konbini::app
