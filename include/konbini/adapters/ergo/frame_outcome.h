#pragma once

#include <cstdint>

#include "pictor/surface/frame_result.h"

// @implements spec/interface/pictor-rendering.md Surface / device recovery
// @implements spec/interface/ergo-runtime.md Render host

namespace konbini::adapters::ergo {

// One frame's result as the app lifecycle sees it. It is derived only from
// Pictor's typed `FrameResult` (pinned Pictor 02ea861c) and from whether the
// pinned Ergo `FrameComposer` presented, so loss is never inferred from
// swapchain handles or a device-idle probe.
enum class FrameOutcome : std::uint8_t {
    // Normal frame: acquire, submit and present happened.
    Presented = 0,
    // The frame was presented and Pictor then rebuilt the swapchain
    // (out-of-date / suboptimal present). Swapchain dependents are stale.
    PresentedAfterRebuild,
    // Acquire reported `RecreateSwapchain` and Pictor rebuilt the swapchain.
    // No submission happened; dependents must be rebuilt before reuse.
    SkippedForRebuild,
    // `RecreateSwapchain` without a replacement (zero-area surface). Pictor
    // kept the old swapchain; the host retries the rebuild after restore.
    SkippedWhileMinimized,
    // Presentation is suspended by the host lifecycle (pause / background /
    // surface released). No native acquire, submit or present was issued.
    Suspended,
    // The native surface is gone (`VK_ERROR_SURFACE_LOST_KHR` or no native
    // window). Requires teardown + reinitialize against a native surface.
    SurfaceLost,
    // The device is gone. Requires teardown + reinitialize; never a resize.
    DeviceLost,
    // `NotInitialized` / `Error`, or a result the frame contract forbids.
    RenderFailed,
};

// Maps Pictor's typed result for the frame to the app-facing outcome.
// `result` is either the pre-frame gate (`gate_frame`) or
// `VulkanContext::last_frame_result()` after the Ergo frame; `presented`
// is whether `FrameComposer::frame_count()` advanced.
[[nodiscard]] FrameOutcome classifyFrameResult(
    const ::pictor::FrameResult& result, bool presented) noexcept;

// The frame was shown on screen (the game may count it as presented).
[[nodiscard]] bool wasPresented(FrameOutcome outcome) noexcept;

// Swapchain-dependent resources (composite descriptors, scene targets,
// composer passes) must be rebuilt against the replacement swapchain.
[[nodiscard]] bool requiresDependentRebuild(FrameOutcome outcome) noexcept;

// Only an explicit render teardown + reinitialize clears this outcome
// (Pictor spec/feature/portability/mobile-surface-recovery.md 5.2 / 5.3).
// Swapchain recreation must not be attempted for it.
[[nodiscard]] bool requiresReinitialize(FrameOutcome outcome) noexcept;

[[nodiscard]] const char* describeFrameOutcome(FrameOutcome outcome) noexcept;

}  // namespace konbini::adapters::ergo
