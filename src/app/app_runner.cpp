#include "konbini/app/app_runner.h"
#include "konbini/app/game_session.h"
#include <cstdio>
#include <utility>
#include "konbini/app/executable_directory.h"
#include "ergo/frame/frame.h"
#include "ergo/input/input_system.h"
#include "konbini/adapters/ergo/ergo_input_bridge.h"
#include "konbini/adapters/ergo/input_action_map.h"
#include "konbini/adapters/ergo/render_device_host.h"
#include "konbini/adapters/ergo/swapchain_identity.h"
#include "konbini/adapters/ergo/world_frame_graph.h"

namespace konbini::app {
struct AppRunner::Impl {
    explicit Impl(AppRunnerConfig configuration)
        : config(std::move(configuration)),playtest(readPlaytestOptions(executableDirectory()/"playtest.cfg")),
          game(config.paths.contentFile,config.maxTicksPerFrame,playtest) {}
    AppRunnerConfig config;
    PlaytestOptions playtest;
    GameSession game;
    adapters::ergo::RenderDeviceHost device;
    adapters::ergo::WorldFrameGraph graph;
    ::ergo::input::InputSystem input;
    adapters::ergo::ErgoInputBridge bridge;
    adapters::ergo::InputActionMap actionMap;
    bool inputInitialized=false;
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
    if(!impl_->playtest.report.empty()) deviceConfig.windowTitle += " [PLAYTEST " +
        std::to_string(static_cast<unsigned>(impl_->playtest.timeScale)) + "X " +
        (impl_->playtest.autoplay?"AUTO]":"MANUAL]");
    deviceConfig.framesInFlight = impl_->config.framesInFlight;
    deviceConfig.validation = impl_->config.validation;
    deviceConfig.shaderDirectory = impl_->config.paths.shaderDirectory.string();
    deviceConfig.assetRoot =
        impl_->config.paths.contentFile.parent_path().string();
    impl_->device.initialize(deviceConfig);

    impl_->graph.initialize(impl_->device);
    impl_->game.uploadGeometry(impl_->graph);

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
            impl_->game.suspend();
            impl_->bridge.endFrame();
            impl_->device.waitEvents(0.05);
            continue;
        }
        if (windowExtent != impl_->device.swapchainExtent()) {
            impl_->game.suspend();
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
            impl_->game.suspend();
            impl_->bridge.endFrame();
            continue;
        }

        FrameInput input = impl_->actionMap.sample(
            impl_->input, impl_->bridge, extent, deltaSeconds);

        impl_->game.frame(input,extent,impl_->graph);

        const adapters::ergo::FrameOutcome outcome =
            impl_->graph.runFrame(static_cast<float>(deltaSeconds));
        if(outcome==adapters::ergo::FrameOutcome::Presented || outcome==adapters::ergo::FrameOutcome::PresentedAfterRebuild)
            impl_->game.notifyPresented();
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
