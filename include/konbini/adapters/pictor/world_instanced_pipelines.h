#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

#ifndef NOGDI
#define NOGDI
#endif
#include <vulkan/vulkan.h>

// @implements spec/interface/pictor-rendering.md World pass recording

namespace konbini::adapters::pictor {

// Push constant of `konbini_world_instanced.vert`: the camera only. Tint and
// model come from the per-instance storage buffer, because Pictor's recorder
// issues one draw per batch without a host hook for per-draw constants.
struct WorldInstancedPushConstants {
    std::array<float, 16> viewProjection{};
};

static_assert(sizeof(WorldInstancedPushConstants) == sizeof(float) * 16);

// Opaque / translucent pipelines for objects drawn through Pictor's
// `CompiledBatchRecorder`, sharing one layout (set 0 = instance storage
// buffer, vertex push constant = view-projection).
//
// Both pipelines share the layout so that the descriptor set and push
// constant bound by the bridge stay valid when the recorder binds either
// pipeline. Depth / blend states match `WorldPipelines` base / overlay.
class WorldInstancedPipelines {
public:
    WorldInstancedPipelines() = default;
    ~WorldInstancedPipelines();

    WorldInstancedPipelines(const WorldInstancedPipelines&) = delete;
    WorldInstancedPipelines& operator=(const WorldInstancedPipelines&) = delete;

    // Reads `konbini_world_instanced.vert.spv` / `konbini_world.frag.spv`.
    // Missing or invalid SPIR-V is an exception, never a fallback shader.
    void initialize(VkDevice device, const std::filesystem::path& shaderDirectory,
                    VkDescriptorSetLayout instanceSetLayout);

    // Rebuilds both pipelines for `renderPass`; the old pair is destroyed only
    // after the new pair exists. The caller guarantees the old pair is not in
    // flight.
    void setRenderPass(VkRenderPass renderPass);

    void shutdown() noexcept;

    [[nodiscard]] bool isInitialized() const noexcept;
    [[nodiscard]] bool hasPipelines() const noexcept;
    [[nodiscard]] VkRenderPass renderPass() const noexcept;
    [[nodiscard]] VkPipelineLayout layout() const;
    [[nodiscard]] VkPipeline opaquePipeline() const;
    [[nodiscard]] VkPipeline translucentPipeline() const;

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkPipeline opaque_ = VK_NULL_HANDLE;
    VkPipeline translucent_ = VK_NULL_HANDLE;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    std::vector<std::uint32_t> vertexShader_;
    std::vector<std::uint32_t> fragmentShader_;
};

}  // namespace konbini::adapters::pictor
