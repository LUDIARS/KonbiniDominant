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

// @implements spec/interface/pictor-rendering.md Offscreen world composition

namespace konbini::render {

// vertex shader が読む push constant。`viewProjection` は column-major
// (index = col * 4 + row)、右手系 view 空間、Vulkan clip 空間 (depth 0..1、
// Y 下向き) の orthographic で、`IsometricCamera::viewProjection` が正本。
// `tint` は vertex color へ掛ける draw 単位の presentation 色。
struct WorldPushConstants {
    std::array<float, 16> viewProjection{};
    WorldVertex::ColorRgba tint{1.0F, 1.0F, 1.0F, 1.0F};
};

static_assert(sizeof(WorldPushConstants) == sizeof(float) * 20);
static_assert(sizeof(WorldPushConstants) <= 128,
              "push constants must fit the Vulkan guaranteed minimum");

// world pass の base / overlay 2 本の pipeline とその共有 layout の所有者。
//
// Pictor の `build_graphics_pipeline()` は blend 無し・depth test と depth
// write が連動という前提なので、overlay に必要な「depth test あり /
// depth write なし / alpha blend あり」を表現できない。base 側だけ Pictor の
// helper を使うと 2 本の pipeline 状態が別経路で決まるため、両方ここで組む。
class WorldPipelines {
public:
    WorldPipelines() = default;
    ~WorldPipelines();

    WorldPipelines(const WorldPipelines&) = delete;
    WorldPipelines& operator=(const WorldPipelines&) = delete;

    // SPIR-V を読み、共有 pipeline layout を作る。pipeline 自体は render
    // pass が確定してからでないと作れないので、ここでは作らない。
    void initialize(
        VkDevice device, const std::filesystem::path& shaderDirectory);

    // `renderPass` 用に base / overlay を作り直す。新しい 2 本が揃ってから
    // 旧 2 本を破棄する。呼び出し側は旧 pipeline が in-flight でないことを
    // 保証すること (device idle 待ちはこのクラスでは行わない)。
    void setRenderPass(VkRenderPass renderPass);

    void shutdown() noexcept;

    [[nodiscard]] bool isInitialized() const noexcept;
    [[nodiscard]] bool hasPipelines() const noexcept;
    [[nodiscard]] VkRenderPass renderPass() const noexcept;
    [[nodiscard]] VkPipelineLayout layout() const;
    [[nodiscard]] VkPipeline basePipeline() const;
    [[nodiscard]] VkPipeline overlayPipeline() const;

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkPipeline base_ = VK_NULL_HANDLE;
    VkPipeline overlay_ = VK_NULL_HANDLE;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    std::vector<std::uint32_t> vertexShader_;
    std::vector<std::uint32_t> fragmentShader_;
};

}  // namespace konbini::render
