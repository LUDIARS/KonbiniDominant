#include "konbini/adapters/ergo/swapchain_identity.h"

#include <vector>

#include "pictor/surface/vulkan_context.h"

// @implements spec/interface/pictor-rendering.md Offscreen world composition

namespace konbini::adapters::ergo {

bool SwapchainIdentity::isRenderable() const noexcept {
    return swapchain != VK_NULL_HANDLE && extent.width != 0 &&
           extent.height != 0 && imageCount != 0 && framesInFlight != 0;
}

bool operator==(
    const SwapchainIdentity& left, const SwapchainIdentity& right) noexcept {
    return left.swapchain == right.swapchain &&
           left.defaultRenderPass == right.defaultRenderPass &&
           left.firstImageView == right.firstImageView &&
           left.imageCount == right.imageCount &&
           left.framesInFlight == right.framesInFlight &&
           left.extent.width == right.extent.width &&
           left.extent.height == right.extent.height;
}

bool operator!=(
    const SwapchainIdentity& left, const SwapchainIdentity& right) noexcept {
    return !(left == right);
}

SwapchainIdentity sampleSwapchainIdentity(
    const ::pictor::VulkanContext& context) noexcept {
    SwapchainIdentity identity;
    if (!context.is_initialized()) {
        return identity;
    }
    const std::vector<VkImageView>& views = context.swapchain_image_views();
    identity.swapchain = context.swapchain();
    identity.defaultRenderPass = context.default_render_pass();
    identity.firstImageView = views.empty() ? VK_NULL_HANDLE : views.front();
    identity.imageCount = static_cast<std::uint32_t>(views.size());
    identity.framesInFlight = context.frames_in_flight();
    identity.extent = context.swapchain_extent();
    return identity;
}

// @implements spec/interface/ergo-runtime.md Render host
FrameOutcome classifyFrameOutcome(
    const SwapchainIdentity& before, const SwapchainIdentity& after,
    const bool framePresented, const bool deviceLost) noexcept {
    const bool changed = before != after;

    if (framePresented) {
        // present が out-of-date / suboptimal を見て内部再生成した場合、
        // frame は出ているが依存 resource は古い。
        return changed ? FrameOutcome::PresentedAfterRebuild
                       : FrameOutcome::Presented;
    }

    if (deviceLost) {
        // device lost は swapchain の状態と無関係に致命的。identity の変化
        // より先に判定する。
        return FrameOutcome::DeviceLost;
    }
    if (!after.isRenderable()) {
        // 最小化中は extent 0 で swapchain を作れない。lost と混同すると
        // 最小化のたびに落ちる。
        return FrameOutcome::SkippedWhileMinimized;
    }
    if (changed) {
        return FrameOutcome::SkippedForRebuild;
    }
    // acquire が失敗したのに swapchain が作り直されていない。out-of-date で
    // はないので surface 側の喪失として扱う。
    return FrameOutcome::SurfaceLost;
}

bool requiresDependentRebuild(const FrameOutcome outcome) noexcept {
    return outcome == FrameOutcome::PresentedAfterRebuild ||
           outcome == FrameOutcome::SkippedForRebuild;
}

bool isFatal(const FrameOutcome outcome) noexcept {
    return outcome == FrameOutcome::DeviceLost ||
           outcome == FrameOutcome::SurfaceLost;
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
            return "skipped, window minimized";
        case FrameOutcome::DeviceLost:
            return "device lost";
        case FrameOutcome::SurfaceLost:
            return "surface lost";
    }
    return "unknown frame outcome";
}

bool probeDeviceLost(const VkDevice device) noexcept {
    if (device == VK_NULL_HANDLE) {
        return false;
    }
    return vkDeviceWaitIdle(device) == VK_ERROR_DEVICE_LOST;
}

}  // namespace konbini::adapters::ergo
