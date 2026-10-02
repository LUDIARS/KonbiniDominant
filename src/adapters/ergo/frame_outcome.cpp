#include "konbini/adapters/ergo/frame_outcome.h"

// @implements spec/interface/pictor-rendering.md Surface / device recovery

namespace konbini::adapters::ergo {

// @implements spec/interface/pictor-rendering.md Surface / device recovery
FrameOutcome classifyFrameResult(
    const ::pictor::FrameResult& result, const bool presented) noexcept {
    // Loss and suspension come from the status alone. Ergo counts a frame as
    // presented even when the present itself reported a loss, so the status
    // must win over `presented`.
    switch (result.status) {
        case ::pictor::FrameStatus::Ready:
            // A gated-in frame that Ergo did not present means record/submit
            // or the frame contract broke. Do not loop on it as a skip.
            return presented ? FrameOutcome::Presented
                             : FrameOutcome::RenderFailed;
        case ::pictor::FrameStatus::RecreateSwapchain:
            if (!result.swapchain_recreated) {
                // Zero-area surface: the old swapchain is intact and nothing
                // dependent changed. Retry after the extent returns.
                return FrameOutcome::SkippedWhileMinimized;
            }
            return presented ? FrameOutcome::PresentedAfterRebuild
                             : FrameOutcome::SkippedForRebuild;
        case ::pictor::FrameStatus::Suspended:
            return FrameOutcome::Suspended;
        case ::pictor::FrameStatus::SurfaceLost:
            return FrameOutcome::SurfaceLost;
        case ::pictor::FrameStatus::DeviceLost:
            return FrameOutcome::DeviceLost;
        case ::pictor::FrameStatus::NotInitialized:
        case ::pictor::FrameStatus::Error:
            return FrameOutcome::RenderFailed;
    }
    return FrameOutcome::RenderFailed;
}

bool wasPresented(const FrameOutcome outcome) noexcept {
    return outcome == FrameOutcome::Presented ||
           outcome == FrameOutcome::PresentedAfterRebuild;
}

bool requiresDependentRebuild(const FrameOutcome outcome) noexcept {
    return outcome == FrameOutcome::PresentedAfterRebuild ||
           outcome == FrameOutcome::SkippedForRebuild;
}

bool requiresReinitialize(const FrameOutcome outcome) noexcept {
    return outcome == FrameOutcome::SurfaceLost ||
           outcome == FrameOutcome::DeviceLost ||
           outcome == FrameOutcome::RenderFailed;
}

const char* describeFrameOutcome(const FrameOutcome outcome) noexcept {
    switch (outcome) {
        case FrameOutcome::Presented:
            return "presented";
        case FrameOutcome::PresentedAfterRebuild:
            return "presented, swapchain rebuilt during present";
        case FrameOutcome::SkippedForRebuild:
            return "skipped, swapchain out of date and rebuilt";
        case FrameOutcome::SkippedWhileMinimized:
            return "skipped, surface has no drawable area";
        case FrameOutcome::Suspended:
            return "skipped, presentation suspended";
        case FrameOutcome::SurfaceLost:
            return "surface lost";
        case FrameOutcome::DeviceLost:
            return "device lost";
        case FrameOutcome::RenderFailed:
            return "render failed";
    }
    return "unknown frame outcome";
}

}  // namespace konbini::adapters::ergo
