#include "android_host.h"

#include <android_native_app_glue.h>
#include <android/configuration.h>
#include <android/log.h>
#include <android/native_window.h>

#include <chrono>
#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>

#include "android_asset_reader.h"
#include "android_touch_input.h"
#include "konbini/app/native_mobile_runtime.h"
#include "konbini/app/platform/android_host_mapping.h"
#include "konbini/app/platform/lifecycle_event.h"
#include "konbini/app/platform/mobile_graphics_profile.h"
#include "konbini/app/platform/packaged_asset_mirror.h"
#include "konbini/app/platform/required_packaged_assets.h"
#include "pictor/surface/android_surface_provider.h"

namespace konbini::android_host {
namespace {

constexpr const char* kLogTag = "KonbiniDominant";
// Callbacks between two drains: window, focus, activity state, metrics and
// thermal changes. Overflow fails the boot instead of dropping a pause.
constexpr std::size_t kLifecycleQueueCapacity = 64;

[[nodiscard]] std::optional<render::ViewportExtent> windowExtent(ANativeWindow* window) {
    if (window == nullptr) {
        return std::nullopt;
    }
    const int width = ANativeWindow_getWidth(window);
    const int height = ANativeWindow_getHeight(window);
    if (width <= 0 || height <= 0) {
        return std::nullopt;
    }
    return render::ViewportExtent{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
}

}  // namespace

AndroidHost::AndroidHost(android_app* app)
    : app_(app), events_(kLifecycleQueueCapacity), thermal_(events_) {}

AndroidHost::~AndroidHost() {
    detachWindow();
}

// @implements spec/interface/mobile-platform.md Assets and generated geometry
// @implements spec/interface/mobile-platform.md Surface and renderer
void AndroidHost::boot() {
    const app::AndroidAppDirectories directories =
        app::androidAppDirectories(app_->activity->internalDataPath != nullptr
                                       ? std::filesystem::path(app_->activity->internalDataPath)
                                       : std::filesystem::path());
    const app::WritableRoots roots = app::androidWritableRoots(directories);
    app::createWritableRoots(roots);
    paths_.emplace(roots);

    // The APK is the read-only source. Missing content / SPIR-V fails here,
    // naming every missing asset, before any game state exists.
    const AndroidAssetReader reader(app_->activity->assetManager);
    const auto required = app::requiredPackagedAssets();
    const std::filesystem::path mirror = app::androidPackageMirrorRoot(directories);
    app::materializePackagedAssets(reader, required, mirror);

    const app::MobileGraphicsProfileSelection selection = app::selectMobileGraphicsProfile(thermal_.current());
    diagnostic_.emplace(*paths_, selection);
    const std::string described = app::describeMobileGraphicsProfileSelection(selection);
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "%s", described.c_str());

    // Figmentum planCity() + one polygonize pass per facility recipe, here in
    // the device process and before the first frame.
    game_ = std::make_unique<app::NativeMobileRuntime>(
        mirror, app::mobileGraphicsProfile(selection.profile).facilityMesh);
    // Inactive until the activity is resumed and focused.
    push(app::LifecycleEvent::pause());
}

void AndroidHost::onCommand(const std::int32_t command) {
    if (failed_ || !game_) {
        return;
    }
    try {
        switch (command) {
            case APP_CMD_INIT_WINDOW:
                attachWindow();
                break;
            case APP_CMD_TERM_WINDOW:
                // The window is invalid once this callback returns: release
                // GPU work synchronously, then record the loss in order.
                push(app::LifecycleEvent::surfaceLost());
                detachWindow();
                break;
            case APP_CMD_WINDOW_RESIZED:
            case APP_CMD_CONFIG_CHANGED:
            case APP_CMD_CONTENT_RECT_CHANGED:
                if (const auto metrics = displayMetrics(); metrics && surface_) {
                    push(app::LifecycleEvent::displayChanged(*metrics));
                }
                break;
            case APP_CMD_GAINED_FOCUS:
            case APP_CMD_LOST_FOCUS:
                focused_ = command == APP_CMD_GAINED_FOCUS;
                forwardActivityPause();
                break;
            case APP_CMD_RESUME:
            case APP_CMD_PAUSE:
                resumed_ = command == APP_CMD_RESUME;
                forwardActivityPause();
                break;
            case APP_CMD_START:
                push(app::LifecycleEvent::enterForeground());
                break;
            case APP_CMD_STOP:
                push(app::LifecycleEvent::enterBackground());
                break;
            case APP_CMD_LOW_MEMORY:
                push(app::LifecycleEvent::memoryPressure(app::MemoryPressureLevel::Critical));
                break;
            default:
                break;
        }
    } catch (const std::exception& error) {
        fail(error.what());
    }
}

std::int32_t AndroidHost::onInput(const AInputEvent* event) {
    if (failed_ || !game_) {
        return 0;
    }
    try {
        return forwardMotionEvent(event, *game_);
    } catch (const std::exception& error) {
        fail(error.what());
        return 1;
    }
}

void AndroidHost::step() {
    if (failed_ || !game_) {
        return;
    }
    try {
        drainLifecycle();
        if (wantsFrames()) {
            runFrame();
        }
    } catch (const std::exception& error) {
        fail(error.what());
    }
}

bool AndroidHost::wantsFrames() const noexcept {
    return !failed_ && surface_ && state_.advancesSimulation();
}

bool AndroidHost::failed() const noexcept {
    return failed_;
}

// @implements spec/interface/pictor-rendering.md Surface / device recovery
void AndroidHost::attachWindow() {
    const auto extent = windowExtent(app_->window);
    if (!extent) {
        throw std::runtime_error("Android window has no drawable extent");
    }
    game_->detach();
    surface_ = std::make_unique<pictor::AndroidSurfaceProvider>(app_->window, extent->width, extent->height);
    // Throws `RenderInitError` with Pictor's typed status when a required
    // Vulkan capability is missing: that is a failed boot, never a skip.
    game_->attach(*surface_, *extent, density());
    const auto metrics = displayMetrics();
    if (!metrics) {
        throw std::runtime_error("Android window metrics are unavailable");
    }
    push(app::LifecycleEvent::surfaceAvailable(*metrics));
}

void AndroidHost::detachWindow() noexcept {
    if (game_) {
        game_->detach();
    }
    surface_.reset();
}

// Activity pause and focus loss are both "inactive"; only the combined edge
// reaches the shared state so a resume with focus still lost stays paused.
void AndroidHost::forwardActivityPause() {
    const bool paused = !(focused_ && resumed_);
    if (paused == pauseForwarded_) {
        return;
    }
    pauseForwarded_ = paused;
    push(paused ? app::LifecycleEvent::pause() : app::LifecycleEvent::resume());
}

// @implements spec/interface/mobile-platform.md Lifecycle
void AndroidHost::drainLifecycle() {
    const auto batch = events_.drain();
    if (batch.empty()) {
        return;
    }
    const app::LifecycleEffects effects = state_.apply(batch);
    if (effects.display && surface_ && app_->window != nullptr) {
        surface_->update_window(app_->window, effects.display->extentPixels.width,
                                effects.display->extentPixels.height);
        game_->displayMetrics(*effects.display);
    }
    if (effects.memoryPressure) {
        const std::size_t evicted = game_->memoryPressure(*effects.memoryPressure);
        __android_log_print(ANDROID_LOG_INFO, kLogTag, "memory pressure: evicted %zu derived facility meshes",
                            evicted);
    }
    if (effects.thermal) {
        diagnostic_->recordThermalChange(*effects.thermal);
    }
    if (effects.checkpointRequested) {
        // No canonical save format exists yet (TBD-SAVE); the simulation is
        // held, not reset, so nothing authoritative is lost here.
        __android_log_print(ANDROID_LOG_INFO, kLogTag, "background: checkpoint requested, no save format yet");
    }
    game_->pause(!state_.advancesSimulation());
}

// @implements spec/interface/pictor-rendering.md Surface / device recovery
void AndroidHost::runFrame() {
    const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    game_->frame(now);
    if (game_->renderState() == adapters::ergo::RenderLifecycleState::ReinitializeRequired) {
        // Surface / device loss with the window still owned by the host:
        // explicit teardown + reinitialize, never a resize.
        __android_log_print(ANDROID_LOG_WARN, kLogTag, "render %s; reinitializing",
                            adapters::ergo::describeFrameOutcome(game_->renderLossCause()));
        game_->reinitializeRender();
    }
}

void AndroidHost::push(const app::LifecycleEvent& event) {
    // Overflow is latched and thrown by the next drain on this thread.
    static_cast<void>(events_.push(event));
}

std::optional<app::DisplayMetrics> AndroidHost::displayMetrics() const {
    const auto extent = windowExtent(app_->window);
    if (!extent) {
        return std::nullopt;
    }
    const ARect& content = app_->contentRect;
    return app::DisplayMetrics{
        .extentPixels = *extent,
        .density = density(),
        .safeArea = app::safeAreaFromContentRect(*extent, content.left, content.top, content.right, content.bottom),
    };
}

double AndroidHost::density() const {
    const auto value = AConfiguration_getDensity(app_->config);
    return value > 0 && value < 1000 ? value / 160.0 : 1.0;
}

void AndroidHost::fail(const char* message) noexcept {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag, "%s", message);
    failed_ = true;
    detachWindow();
    ANativeActivity_finish(app_->activity);
}

}  // namespace konbini::android_host
