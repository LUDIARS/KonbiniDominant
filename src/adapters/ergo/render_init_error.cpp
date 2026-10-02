#include "konbini/adapters/ergo/render_init_error.h"

// @implements spec/interface/pictor-rendering.md Surface / device recovery

namespace konbini::adapters::ergo {
namespace {

std::string formatMessage(const ::pictor::ContextInitResult& result) {
    std::string message = "failed to initialize the Pictor Vulkan context: ";
    message += describeContextInitStatus(result.status);
    if (!result.detail.empty()) {
        message += " (";
        message += result.detail;
        message += ')';
    }
    return message;
}

}  // namespace

RenderInitError::RenderInitError(const ::pictor::ContextInitResult& result)
    : std::runtime_error(formatMessage(result)), status_(result.status) {}

bool RenderInitError::waitsForSurface() const noexcept {
    return status_ == ::pictor::ContextInitStatus::SurfaceUnavailable;
}

const char* describeContextInitStatus(
    const ::pictor::ContextInitStatus status) noexcept {
    switch (status) {
        case ::pictor::ContextInitStatus::Ok:
            return "ok";
        case ::pictor::ContextInitStatus::NotInitialized:
            return "not initialized";
        case ::pictor::ContextInitStatus::SurfaceUnavailable:
            return "native surface unavailable";
        case ::pictor::ContextInitStatus::MissingInstanceExtension:
            return "missing instance extension";
        case ::pictor::ContextInitStatus::MissingDeviceExtension:
            return "missing device extension";
        case ::pictor::ContextInitStatus::MissingCapability:
            return "missing device capability";
        case ::pictor::ContextInitStatus::NoSuitableDevice:
            return "no suitable device";
        case ::pictor::ContextInitStatus::Failed:
            return "initialization failed";
    }
    return "unknown initialization status";
}

}  // namespace konbini::adapters::ergo
