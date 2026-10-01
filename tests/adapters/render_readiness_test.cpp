#include <cstdio>
#include <cstdlib>

#include "ergo/render/render_backend.h"
#include "ergo/render/render_context.h"
#include "pictor/surface/surface_provider.h"
#include "pictor/surface/vulkan_context.h"

#include "konbini/adapters/ergo/render_readiness.h"

// Consumer contract of the pinned Ergo render backend. No Vulkan device is
// created: the checks only look at the build contract and at which borrowed
// handles the RenderContext carries.
// @implements spec/test/verification-strategy.md 1. Data / ID unit tests
// @implements spec/interface/ergo-runtime.md Render readiness

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

using ::ergo::render::RenderBackendError;
using ::ergo::render::RenderContext;
using konbini::adapters::ergo::RenderUnavailableError;
using konbini::adapters::ergo::expectedRenderPlatform;
using konbini::adapters::ergo::requireRealRenderBackend;
using konbini::adapters::ergo::requireRenderReady;

// Stands in for GlfwSurfaceProvider / AndroidSurfaceProvider /
// IOSSurfaceProvider: Ergo must only see the ISurfaceProvider boundary.
class FakeSurfaceProvider final : public ::pictor::ISurfaceProvider {
public:
    explicit FakeSurfaceProvider(const bool hasWindow) : hasWindow_(hasWindow) {}

    ::pictor::NativeWindowHandle get_native_handle() const override {
        ::pictor::NativeWindowHandle handle;
        if (hasWindow_) {
            handle.type = ::pictor::NativeWindowHandle::Type::Win32;
            handle.win32.hwnd = const_cast<bool*>(&hasWindow_);
        }
        return handle;
    }

    ::pictor::SwapchainConfig get_swapchain_config() const override {
        return {};
    }

private:
    bool hasWindow_ = false;
};

// Returns the reason carried by the thrown error, or None when nothing threw.
RenderBackendError readinessOf(const RenderContext& context) {
    try {
        requireRenderReady(context, "test");
    } catch (const RenderUnavailableError& error) {
        return error.reason();
    }
    return RenderBackendError::None;
}

void testPinnedErgoSelectsRealRenderForThisPlatform() {
    const auto contract = ::ergo::render::render_backend_contract();
    CHECK(contract.real_render_enabled);
    CHECK(contract.platform == expectedRenderPlatform());
    bool threw = false;
    try {
        requireRealRenderBackend("test");
    } catch (const RenderUnavailableError&) {
        threw = true;
    }
    CHECK(!threw);
}

void testTypedFailureIsNotSwallowed() {
    bool threw = false;
    try {
        requireRenderReady(RenderBackendError::FrameSubmissionFailed, "test");
    } catch (const RenderUnavailableError& error) {
        threw = true;
        CHECK(error.reason() == RenderBackendError::FrameSubmissionFailed);
    }
    CHECK(threw);

    threw = false;
    try {
        requireRenderReady(RenderBackendError::None, "test");
    } catch (const RenderUnavailableError&) {
        threw = true;
    }
    CHECK(!threw);
}

void testMissingVulkanContextIsRejected() {
    FakeSurfaceProvider surface(true);
    RenderContext context;
    context.surface = &surface;
    CHECK(readinessOf(context) == RenderBackendError::VulkanContextMissing);
}

// Pre-b618de7 consumers left `surface` null on mobile. With the
// platform-neutral contract that is a startup error, not a headless success.
void testMissingSurfaceProviderIsRejected() {
    ::pictor::VulkanContext vulkan;
    RenderContext context;
    context.vk = &vulkan;
    CHECK(readinessOf(context) == RenderBackendError::SurfaceProviderMissing);
}

void testProviderWithoutNativeWindowIsNotReady() {
    ::pictor::VulkanContext vulkan;
    FakeSurfaceProvider surface(false);
    RenderContext context;
    context.vk = &vulkan;
    context.surface = &surface;
    CHECK(readinessOf(context) == RenderBackendError::SurfaceNotReady);
}

void testBorrowedProviderWithWindowIsReady() {
    ::pictor::VulkanContext vulkan;
    FakeSurfaceProvider surface(true);
    RenderContext context;
    context.vk = &vulkan;
    context.surface = &surface;
    CHECK(readinessOf(context) == RenderBackendError::None);
}

}  // namespace

int main() {
    testPinnedErgoSelectsRealRenderForThisPlatform();
    testTypedFailureIsNotSwallowed();
    testMissingVulkanContextIsRejected();
    testMissingSurfaceProviderIsRejected();
    testProviderWithoutNativeWindowIsNotReady();
    testBorrowedProviderWithWindowIsReady();

    if (g_failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failures);
        return EXIT_FAILURE;
    }
    std::fprintf(stdout, "all checks passed\n");
    return EXIT_SUCCESS;
}
