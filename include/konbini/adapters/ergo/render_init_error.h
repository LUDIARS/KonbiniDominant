#pragma once

#include <stdexcept>
#include <string>

#include "pictor/surface/context_init_result.h"

// @implements spec/interface/pictor-rendering.md Surface / device recovery

namespace konbini::adapters::ergo {

// Pictor `VulkanContext::initialize()` failure, keeping its typed status so a
// host can tell "native window not there yet" from a capability gap. Missing
// extensions / capabilities are never folded into a resize or a frame skip.
class RenderInitError final : public std::runtime_error {
public:
    explicit RenderInitError(const ::pictor::ContextInitResult& result);

    [[nodiscard]] ::pictor::ContextInitStatus status() const noexcept {
        return status_;
    }
    // Only `SurfaceUnavailable` may be retried, and only after the host hands
    // the provider a native window again. Every other status is a fault.
    [[nodiscard]] bool waitsForSurface() const noexcept;

private:
    ::pictor::ContextInitStatus status_;
};

[[nodiscard]] const char* describeContextInitStatus(
    ::pictor::ContextInitStatus status) noexcept;

}  // namespace konbini::adapters::ergo
