#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

#include "konbini/render/world_vertex.h"

#ifndef NOGDI
#define NOGDI
#endif
#include <vulkan/vulkan.h>

// @implements spec/feature/ui-ux.md Common HUD

namespace konbini::render {

// vertex shader が読む push constant。HUD geometry は pixel 空間なので、
// NDC への変換に framebuffer の実サイズが要る。
struct HudPushConstants {
    std::array<float, 4> viewportPixels{1.0F, 1.0F, 0.0F, 0.0F};
};

static_assert(sizeof(HudPushConstants) == sizeof(float) * 4);

// HUD pass の pipeline 1 本とその layout の所有者。Pictor の
// `build_graphics_pipeline()` は blend を持てず、HUD は panel と glyph を
// alpha 合成するため、world pass と同じくここで組む。
class HudPipelines {
public:
    HudPipelines() = default;
    ~HudPipelines();

    HudPipelines(const HudPipelines&) = delete;
    HudPipelines& operator=(const HudPipelines&) = delete;

    void initialize(
        VkDevice device, const std::filesystem::path& shaderDirectory);

    // `renderPass` 用に pipeline を作り直す。新しい 1 本が出来てから旧世代を
    // 捨てる。旧 pipeline が in-flight でないことの保証は呼び出し側の責務。
    void setRenderPass(VkRenderPass renderPass);

    void shutdown() noexcept;

    [[nodiscard]] bool isInitialized() const noexcept;
    [[nodiscard]] bool hasPipeline() const noexcept;
    [[nodiscard]] VkRenderPass renderPass() const noexcept;
    [[nodiscard]] VkPipelineLayout layout() const;
    [[nodiscard]] VkPipeline pipeline() const;

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    std::vector<std::uint32_t> vertexShader_;
    std::vector<std::uint32_t> fragmentShader_;
};

}  // namespace konbini::render
