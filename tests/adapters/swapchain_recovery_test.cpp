#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

#include "ergo/render/render_context.h"
#include "pictor/surface/frame_gate.h"

#include "konbini/adapters/ergo/frame_outcome.h"
#include "konbini/adapters/ergo/layer_initialization_scope.h"
#include "konbini/adapters/ergo/render_init_error.h"
#include "konbini/adapters/ergo/render_lifecycle.h"
#include "konbini/adapters/ergo/tracked_render_layer.h"

// @implements spec/test/verification-strategy.md 1. Data / ID unit tests
// @implements spec/interface/ergo-runtime.md Render host
// @implements spec/interface/pictor-rendering.md Surface / device recovery

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

using konbini::adapters::ergo::classifyFrameResult;
using konbini::adapters::ergo::FrameOutcome;
using konbini::adapters::ergo::LayerInitializationScope;
using konbini::adapters::ergo::RenderInitError;
using konbini::adapters::ergo::RenderLifecycle;
using konbini::adapters::ergo::RenderLifecycleState;
using konbini::adapters::ergo::requiresDependentRebuild;
using konbini::adapters::ergo::requiresReinitialize;
using konbini::adapters::ergo::TrackedRenderLayer;
using konbini::adapters::ergo::wasPresented;

::pictor::FrameResult makeResult(
    const ::pictor::FrameStatus status, const bool recreated = false) {
    ::pictor::FrameResult result;
    result.status = status;
    result.swapchain_recreated = recreated;
    return result;
}

// --- frame outcome ------------------------------------------------------
// Pinned Pictor 02ea861c reports a typed FrameResult. Every status must map
// to exactly one app outcome, and a loss must never look like a resize.

void testReadyPresentedFrameIsNotARebuild() {
    const FrameOutcome outcome =
        classifyFrameResult(makeResult(::pictor::FrameStatus::Ready), true);
    CHECK(outcome == FrameOutcome::Presented);
    CHECK(wasPresented(outcome));
    CHECK(!requiresDependentRebuild(outcome));
    CHECK(!requiresReinitialize(outcome));
}

void testReadyWithoutPresentIsAFailureNotASkip() {
    const FrameOutcome outcome =
        classifyFrameResult(makeResult(::pictor::FrameStatus::Ready), false);
    CHECK(outcome == FrameOutcome::RenderFailed);
    CHECK(requiresReinitialize(outcome));
}

void testPresentThatRecreatedSwapchainRequiresRebuild() {
    const FrameOutcome outcome = classifyFrameResult(
        makeResult(::pictor::FrameStatus::RecreateSwapchain, true), true);
    CHECK(outcome == FrameOutcome::PresentedAfterRebuild);
    CHECK(wasPresented(outcome));
    CHECK(requiresDependentRebuild(outcome));
    CHECK(!requiresReinitialize(outcome));
}

void testOutOfDateAcquireIsRecoverable() {
    const FrameOutcome outcome = classifyFrameResult(
        makeResult(::pictor::FrameStatus::RecreateSwapchain, true), false);
    CHECK(outcome == FrameOutcome::SkippedForRebuild);
    CHECK(!wasPresented(outcome));
    CHECK(requiresDependentRebuild(outcome));
    CHECK(!requiresReinitialize(outcome));
}

void testZeroAreaRecreateKeepsTheOldSwapchain() {
    // Pictor keeps the old swapchain and reports no replacement; dependents
    // still match it, so nothing may be rebuilt yet.
    for (const bool presented : {false, true}) {
        const FrameOutcome outcome = classifyFrameResult(
            makeResult(::pictor::FrameStatus::RecreateSwapchain, false),
            presented);
        CHECK(outcome == FrameOutcome::SkippedWhileMinimized);
        CHECK(!requiresDependentRebuild(outcome));
        CHECK(!requiresReinitialize(outcome));
    }
}

void testSuspendedIssuesNoWorkAndNeedsNoRecovery() {
    const FrameOutcome outcome = classifyFrameResult(
        makeResult(::pictor::FrameStatus::Suspended), false);
    CHECK(outcome == FrameOutcome::Suspended);
    CHECK(!wasPresented(outcome));
    CHECK(!requiresDependentRebuild(outcome));
    CHECK(!requiresReinitialize(outcome));
}

void testLossWinsOverPresentAndSwapchainState() {
    // Ergo counts a frame whose present reported a loss as presented. The
    // typed status must still win, with or without a recreated swapchain.
    for (const bool presented : {false, true}) {
        for (const bool recreated : {false, true}) {
            const FrameOutcome surface = classifyFrameResult(
                makeResult(::pictor::FrameStatus::SurfaceLost, recreated),
                presented);
            const FrameOutcome device = classifyFrameResult(
                makeResult(::pictor::FrameStatus::DeviceLost, recreated),
                presented);
            CHECK(surface == FrameOutcome::SurfaceLost);
            CHECK(device == FrameOutcome::DeviceLost);
            CHECK(!requiresDependentRebuild(surface));
            CHECK(!requiresDependentRebuild(device));
            CHECK(requiresReinitialize(surface));
            CHECK(requiresReinitialize(device));
            CHECK(!wasPresented(device));
        }
    }
}

void testErrorAndNotInitializedRequireReinitialize() {
    for (const auto status : {::pictor::FrameStatus::Error,
                              ::pictor::FrameStatus::NotInitialized}) {
        const FrameOutcome outcome =
            classifyFrameResult(makeResult(status), false);
        CHECK(outcome == FrameOutcome::RenderFailed);
        CHECK(requiresReinitialize(outcome));
        CHECK(!requiresDependentRebuild(outcome));
    }
}

void testRequiresReinitializeCoversPictorsSet() {
    // KD must not be looser than Pictor's own "explicit reinitialize" set.
    for (const auto status :
         {::pictor::FrameStatus::Ready, ::pictor::FrameStatus::RecreateSwapchain,
          ::pictor::FrameStatus::SurfaceLost, ::pictor::FrameStatus::DeviceLost,
          ::pictor::FrameStatus::NotInitialized, ::pictor::FrameStatus::Error,
          ::pictor::FrameStatus::Suspended}) {
        if (::pictor::frame_status_requires_reinitialize(status)) {
            CHECK(requiresReinitialize(
                classifyFrameResult(makeResult(status, true), true)));
        }
    }
}

void testGateBlocksSubmissionBeforeAnyNativeCall() {
    // The pre-frame gate is Pictor's own predicate. Suspension wins over a
    // missing window, and a latched loss wins over suspension.
    ::pictor::FrameGateInput input;
    input.initialized = true;
    input.latched = ::pictor::FrameStatus::Ready;
    input.native_surface_available = true;
    CHECK(::pictor::gate_frame(input).status == ::pictor::FrameStatus::Ready);

    input.presentation_suspended = true;
    input.native_surface_available = false;
    CHECK(classifyFrameResult(::pictor::gate_frame(input), false) ==
          FrameOutcome::Suspended);

    input.presentation_suspended = false;
    CHECK(classifyFrameResult(::pictor::gate_frame(input), false) ==
          FrameOutcome::SurfaceLost);

    input.presentation_suspended = true;
    input.latched = ::pictor::FrameStatus::DeviceLost;
    CHECK(classifyFrameResult(::pictor::gate_frame(input), false) ==
          FrameOutcome::DeviceLost);
}

// --- render lifecycle ---------------------------------------------------

RenderLifecycle makeRunningLifecycle() {
    RenderLifecycle lifecycle;
    lifecycle.attached();
    return lifecycle;
}

void testLifecycleStartsDetachedAndRunsAfterAttach() {
    RenderLifecycle lifecycle;
    CHECK(lifecycle.state() == RenderLifecycleState::Detached);
    CHECK(!lifecycle.maySubmit());
    CHECK(lifecycle.presentationSuspended());
    lifecycle.attached();
    CHECK(lifecycle.state() == RenderLifecycleState::Running);
    CHECK(lifecycle.maySubmit());
    CHECK(!lifecycle.presentationSuspended());
}

void testPauseAndZeroAreaSuspendPresentation() {
    RenderLifecycle lifecycle = makeRunningLifecycle();
    lifecycle.setPaused(true);
    CHECK(lifecycle.state() == RenderLifecycleState::Suspended);
    CHECK(lifecycle.presentationSuspended());
    lifecycle.setPaused(false);
    lifecycle.setDrawableArea(false);
    CHECK(!lifecycle.maySubmit());
    lifecycle.setDrawableArea(true);
    CHECK(lifecycle.maySubmit());
}

void testSurfaceLostStopsSubmissionUntilReinitialize() {
    RenderLifecycle lifecycle = makeRunningLifecycle();
    lifecycle.observe(FrameOutcome::SurfaceLost);
    CHECK(lifecycle.state() == RenderLifecycleState::ReinitializeRequired);
    CHECK(!lifecycle.maySubmit());
    CHECK(lifecycle.presentationSuspended());
    // Resume, resize or a later recoverable frame cannot clear the loss.
    lifecycle.setPaused(false);
    lifecycle.setDrawableArea(true);
    lifecycle.observe(FrameOutcome::SkippedForRebuild);
    lifecycle.observe(FrameOutcome::Presented);
    CHECK(lifecycle.state() == RenderLifecycleState::ReinitializeRequired);
    // Teardown + reinitialize is the only way back.
    lifecycle.detached();
    CHECK(lifecycle.state() == RenderLifecycleState::Detached);
    lifecycle.attached();
    CHECK(lifecycle.state() == RenderLifecycleState::Running);
    CHECK(lifecycle.lossCause() == FrameOutcome::Presented);
}

void testDeviceLostIsNotTreatedAsResize() {
    RenderLifecycle lifecycle = makeRunningLifecycle();
    lifecycle.observe(FrameOutcome::DeviceLost);
    CHECK(lifecycle.state() == RenderLifecycleState::ReinitializeRequired);
    // A later latched loss must not hide the original cause.
    lifecycle.observe(FrameOutcome::SurfaceLost);
    CHECK(lifecycle.lossCause() == FrameOutcome::DeviceLost);
}

void testResizeClassOutcomesKeepRunning() {
    RenderLifecycle lifecycle = makeRunningLifecycle();
    for (const auto outcome :
         {FrameOutcome::Presented, FrameOutcome::PresentedAfterRebuild,
          FrameOutcome::SkippedForRebuild, FrameOutcome::SkippedWhileMinimized,
          FrameOutcome::Suspended}) {
        lifecycle.observe(outcome);
        CHECK(lifecycle.state() == RenderLifecycleState::Running);
    }
}

// --- render init status -------------------------------------------------

void testOnlyMissingSurfaceWaitsForTheHost() {
    ::pictor::ContextInitResult result;
    result.status = ::pictor::ContextInitStatus::SurfaceUnavailable;
    CHECK(RenderInitError(result).waitsForSurface());

    for (const auto status :
         {::pictor::ContextInitStatus::MissingInstanceExtension,
          ::pictor::ContextInitStatus::MissingDeviceExtension,
          ::pictor::ContextInitStatus::MissingCapability,
          ::pictor::ContextInitStatus::NoSuitableDevice,
          ::pictor::ContextInitStatus::Failed}) {
        result.status = status;
        result.detail = "VK_KHR_portability_subset";
        const RenderInitError error(result);
        CHECK(!error.waitsForSurface());
        CHECK(error.status() == status);
        // The missing extension / capability name reaches the diagnostic.
        CHECK(std::string(error.what()).find("VK_KHR_portability_subset") !=
              std::string::npos);
    }
}

// --- layer initialization rollback --------------------------------------
// pinned Ergo の FrameComposer は途中失敗した initialize を巻き戻さないので、
// game 側の scope が逆順で shutdown する。

class RecordingLayer final : public ::ergo::render::IRenderLayer {
public:
    RecordingLayer(int id, std::vector<int>& log, bool failOnInitialize)
        : id_(id), log_(&log), failOnInitialize_(failOnInitialize) {}

    void initialize(::ergo::render::RenderContext& /*context*/) override {
        if (failOnInitialize_) {
            throw std::runtime_error("layer initialization failed");
        }
        initialized_ = true;
    }

    void set_render_pass(VkRenderPass /*renderPass*/) override {}
    void record(VkCommandBuffer, VkExtent2D) override {}

    void shutdown() override {
        if (!initialized_) {
            return;
        }
        initialized_ = false;
        log_->push_back(id_);
    }

    [[nodiscard]] bool initialized() const noexcept { return initialized_; }

private:
    int id_ = 0;
    std::vector<int>* log_ = nullptr;
    bool failOnInitialize_ = false;
    bool initialized_ = false;
};

void testRollbackShutsDownInReverseOrder() {
    std::vector<int> shutdownOrder;
    RecordingLayer first(1, shutdownOrder, false);
    RecordingLayer second(2, shutdownOrder, false);
    RecordingLayer failing(3, shutdownOrder, true);

    ::ergo::render::RenderContext context;
    LayerInitializationScope scope;
    TrackedRenderLayer trackedFirst(first, scope);
    TrackedRenderLayer trackedSecond(second, scope);
    TrackedRenderLayer trackedFailing(failing, scope);

    bool threw = false;
    try {
        trackedFirst.initialize(context);
        trackedSecond.initialize(context);
        trackedFailing.initialize(context);
    } catch (const std::runtime_error&) {
        threw = true;
        scope.rollback();
    }
    CHECK(threw);
    CHECK(scope.initializedCount() == 0);
    CHECK(shutdownOrder.size() == 2);
    CHECK(shutdownOrder[0] == 2);
    CHECK(shutdownOrder[1] == 1);
    CHECK(!first.initialized());
    CHECK(!second.initialized());
}

void testShutdownRemovesLayerFromScope() {
    std::vector<int> shutdownOrder;
    RecordingLayer layer(7, shutdownOrder, false);

    ::ergo::render::RenderContext context;
    LayerInitializationScope scope;
    TrackedRenderLayer tracked(layer, scope);
    tracked.initialize(context);
    CHECK(scope.initializedCount() == 1);

    tracked.shutdown();
    CHECK(scope.initializedCount() == 0);
    // rollback が二重に shutdown しないこと。
    scope.rollback();
    CHECK(shutdownOrder.size() == 1);
}

}  // namespace

// @implements spec/test/verification-strategy.md 1. Data / ID unit tests
// @spec 1. Data / ID unit tests
int main() {
    testReadyPresentedFrameIsNotARebuild();
    testReadyWithoutPresentIsAFailureNotASkip();
    testPresentThatRecreatedSwapchainRequiresRebuild();
    testOutOfDateAcquireIsRecoverable();
    testZeroAreaRecreateKeepsTheOldSwapchain();
    testSuspendedIssuesNoWorkAndNeedsNoRecovery();
    testLossWinsOverPresentAndSwapchainState();
    testErrorAndNotInitializedRequireReinitialize();
    testRequiresReinitializeCoversPictorsSet();
    testGateBlocksSubmissionBeforeAnyNativeCall();
    testLifecycleStartsDetachedAndRunsAfterAttach();
    testPauseAndZeroAreaSuspendPresentation();
    testSurfaceLostStopsSubmissionUntilReinitialize();
    testDeviceLostIsNotTreatedAsResize();
    testResizeClassOutcomesKeepRunning();
    testOnlyMissingSurfaceWaitsForTheHost();
    testRollbackShutsDownInReverseOrder();
    testShutdownRemovesLayerFromScope();

    if (g_failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failures);
        return EXIT_FAILURE;
    }
    std::fprintf(stdout, "all checks passed\n");
    return EXIT_SUCCESS;
}
