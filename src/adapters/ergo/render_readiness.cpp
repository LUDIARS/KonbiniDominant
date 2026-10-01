#include "konbini/adapters/ergo/render_readiness.h"

#include <string>

#include "ergo/render/render_requirements.h"

namespace konbini::adapters::ergo {
namespace {

std::string describe(const ::ergo::render::RenderBackendError reason,
                     const char* const stage) {
    std::string message = "real render path unavailable at ";
    message += stage != nullptr ? stage : "(unknown stage)";
    message += ": ";
    message += ::ergo::render::to_string(reason);
    return message;
}

}  // namespace

RenderUnavailableError::RenderUnavailableError(
    const ::ergo::render::RenderBackendError reason, const char* const stage)
    : std::runtime_error(describe(reason, stage)), reason_(reason) {}

::ergo::render::RenderBackendError RenderUnavailableError::reason()
    const noexcept {
    return reason_;
}

::ergo::render::RenderPlatform expectedRenderPlatform() noexcept {
#if defined(__ANDROID__)
    return ::ergo::render::RenderPlatform::Android;
#elif defined(KONBINI_PLATFORM_IOS)
    return ::ergo::render::RenderPlatform::IOS;
#else
    return ::ergo::render::RenderPlatform::Desktop;
#endif
}

// @implements spec/interface/ergo-runtime.md Render readiness
void requireRealRenderBackend(const char* const stage) {
    const ::ergo::render::RenderBackendContract contract =
        ::ergo::render::render_backend_contract();
    if (!contract.real_render_enabled ||
        contract.platform != expectedRenderPlatform()) {
        throw RenderUnavailableError(
            ::ergo::render::RenderBackendError::VulkanBackendUnavailable,
            stage);
    }
}

void requireRenderReady(const ::ergo::render::RenderBackendError reason,
                        const char* const stage) {
    if (reason != ::ergo::render::RenderBackendError::None) {
        throw RenderUnavailableError(reason, stage);
    }
}

void requireRenderReady(const ::ergo::render::RenderContext& context,
                        const char* const stage) {
    requireRealRenderBackend(stage);
    requireRenderReady(::ergo::render::check_render_requirements(context),
                       stage);
}

}  // namespace konbini::adapters::ergo
