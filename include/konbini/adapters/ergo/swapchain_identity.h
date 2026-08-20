#pragma once

#include <cstdint>

#ifndef NOGDI
#define NOGDI
#endif
#include <vulkan/vulkan.h>

namespace pictor {
class VulkanContext;
}

// @implements spec/interface/pictor-rendering.md Offscreen world composition
// @implements spec/interface/ergo-runtime.md Render host

namespace konbini::adapters::ergo {

// swapchain に紐づく資源の同一性。Pictor は再生成を通知しないので、frame の
// 前後でこの値を比較して「作り直されたか」を検出する。
//
// 単体の handle 比較だけでは不足する: driver が同じ address を再利用しうる
// ので、extent / image 数 / 既定 render pass も併せて見る。
struct SwapchainIdentity {
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkRenderPass defaultRenderPass = VK_NULL_HANDLE;
    VkImageView firstImageView = VK_NULL_HANDLE;
    std::uint32_t imageCount = 0;
    std::uint32_t framesInFlight = 0;
    VkExtent2D extent{0, 0};

    [[nodiscard]] bool isRenderable() const noexcept;
};

[[nodiscard]] bool operator==(
    const SwapchainIdentity& left, const SwapchainIdentity& right) noexcept;
[[nodiscard]] bool operator!=(
    const SwapchainIdentity& left, const SwapchainIdentity& right) noexcept;

[[nodiscard]] SwapchainIdentity sampleSwapchainIdentity(
    const ::pictor::VulkanContext& context) noexcept;

// pinned Pictor (c088e8d1) の `acquire_next_image()` は out-of-date (内部で
// 再生成済み) と device / surface lost を同じ `UINT32_MAX` へ畳み、
// `present()` も戻り値を見ずに再生成する。pinned Ergo (771b027f) の
// `run_frame()` はどちらでも `true` を返す。この分類はその情報を frame 前後
// の identity と present 有無から復元するためのもので、混同したまま
// 「回復可能」として回し続けないための入口になる。
enum class FrameOutcome : std::uint8_t {
    // 通常。present まで到達した。
    Presented = 0,
    // present はしたが、その present が swapchain を作り直した (out-of-date
    // / suboptimal)。依存 resource の再構築が要る。
    PresentedAfterRebuild,
    // acquire が out-of-date を返し、Pictor が再生成した。frame は捨てる。
    SkippedForRebuild,
    // window が最小化されて extent が 0。再生成もできないので待つ。
    SkippedWhileMinimized,
    // acquire が失敗し swapchain も変わっていない。回復しないので落とす。
    DeviceLost,
    SurfaceLost,
};

// `framePresented` は `FrameComposer::frame_count()` が進んだかで判定する
// (run_frame の戻り値は skip でも true)。`deviceLost` は
// `probeDeviceLost()` の結果。
[[nodiscard]] FrameOutcome classifyFrameOutcome(
    const SwapchainIdentity& before, const SwapchainIdentity& after,
    bool framePresented, bool deviceLost) noexcept;

// composite descriptor / scene target のように swapchain 世代へ依存する
// resource を作り直す必要がある結果か。
[[nodiscard]] bool requiresDependentRebuild(FrameOutcome outcome) noexcept;

[[nodiscard]] bool isFatal(FrameOutcome outcome) noexcept;

[[nodiscard]] const char* describeFrameOutcome(FrameOutcome outcome) noexcept;

// `vkDeviceWaitIdle` は device lost を返す。surface lost では返さないので、
// 2 つを取り違えずに分類できる。
[[nodiscard]] bool probeDeviceLost(VkDevice device) noexcept;

}  // namespace konbini::adapters::ergo
