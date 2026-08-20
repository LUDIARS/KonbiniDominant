#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>

#include "ergo/render/render_context.h"

#include "konbini/adapters/ergo/layer_initialization_scope.h"
#include "konbini/adapters/ergo/swapchain_identity.h"
#include "konbini/adapters/ergo/tracked_render_layer.h"

// @implements spec/test/verification-strategy.md 1. Data / ID unit tests
// @implements spec/interface/ergo-runtime.md Render host

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

using konbini::adapters::ergo::classifyFrameOutcome;
using konbini::adapters::ergo::FrameOutcome;
using konbini::adapters::ergo::isFatal;
using konbini::adapters::ergo::LayerInitializationScope;
using konbini::adapters::ergo::requiresDependentRebuild;
using konbini::adapters::ergo::SwapchainIdentity;
using konbini::adapters::ergo::TrackedRenderLayer;

// handle は不透明なので、test では非 null な偽値を作って同一性比較だけを見る。
SwapchainIdentity makeIdentity(const std::uint64_t generation) {
    SwapchainIdentity identity;
    identity.swapchain =
        reinterpret_cast<VkSwapchainKHR>(0x1000 + generation);
    identity.defaultRenderPass =
        reinterpret_cast<VkRenderPass>(0x2000 + generation);
    identity.firstImageView =
        reinterpret_cast<VkImageView>(0x3000 + generation);
    identity.imageCount = 3;
    identity.framesInFlight = 2;
    identity.extent = {1280, 720};
    return identity;
}

SwapchainIdentity makeMinimizedIdentity() {
    SwapchainIdentity identity;
    identity.extent = {0, 0};
    return identity;
}

// --- frame outcome ------------------------------------------------------
// pinned Pictor / Ergo は out-of-date と device / surface lost を同じ戻り値へ
// 畳むので、この分類が「回復可能」と「致命」を分ける唯一の判断点になる。

void testPresentedFrameIsNotARebuild() {
    const SwapchainIdentity identity = makeIdentity(0);
    const FrameOutcome outcome =
        classifyFrameOutcome(identity, identity, true, false);
    CHECK(outcome == FrameOutcome::Presented);
    CHECK(!requiresDependentRebuild(outcome));
    CHECK(!isFatal(outcome));
}

void testPresentThatRecreatedSwapchainRequiresRebuild() {
    const FrameOutcome outcome = classifyFrameOutcome(
        makeIdentity(0), makeIdentity(1), true, false);
    CHECK(outcome == FrameOutcome::PresentedAfterRebuild);
    CHECK(requiresDependentRebuild(outcome));
    CHECK(!isFatal(outcome));
}

void testOutOfDateAcquireIsRecoverable() {
    const FrameOutcome outcome = classifyFrameOutcome(
        makeIdentity(0), makeIdentity(1), false, false);
    CHECK(outcome == FrameOutcome::SkippedForRebuild);
    CHECK(requiresDependentRebuild(outcome));
    CHECK(!isFatal(outcome));
}

void testMinimizedWindowIsNotTreatedAsLost() {
    const FrameOutcome outcome = classifyFrameOutcome(
        makeIdentity(0), makeMinimizedIdentity(), false, false);
    CHECK(outcome == FrameOutcome::SkippedWhileMinimized);
    CHECK(!requiresDependentRebuild(outcome));
    CHECK(!isFatal(outcome));
}

void testUnchangedSwapchainAfterFailedAcquireIsSurfaceLost() {
    const SwapchainIdentity identity = makeIdentity(0);
    const FrameOutcome outcome =
        classifyFrameOutcome(identity, identity, false, false);
    CHECK(outcome == FrameOutcome::SurfaceLost);
    CHECK(isFatal(outcome));
}

void testDeviceLostWinsOverSwapchainState() {
    const FrameOutcome outcome = classifyFrameOutcome(
        makeIdentity(0), makeIdentity(1), false, true);
    CHECK(outcome == FrameOutcome::DeviceLost);
    CHECK(isFatal(outcome));
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
    testPresentedFrameIsNotARebuild();
    testPresentThatRecreatedSwapchainRequiresRebuild();
    testOutOfDateAcquireIsRecoverable();
    testMinimizedWindowIsNotTreatedAsLost();
    testUnchangedSwapchainAfterFailedAcquireIsSurfaceLost();
    testDeviceLostWinsOverSwapchainState();
    testRollbackShutsDownInReverseOrder();
    testShutdownRemovesLayerFromScope();

    if (g_failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failures);
        return EXIT_FAILURE;
    }
    std::fprintf(stdout, "all checks passed\n");
    return EXIT_SUCCESS;
}
