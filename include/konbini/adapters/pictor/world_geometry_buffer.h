#pragma once

#include <cstdint>

#include "konbini/render/world_mesh.h"

#ifndef NOGDI
#define NOGDI
#endif
#include <vulkan/vulkan.h>

// @implements spec/interface/pictor-rendering.md Required bridge

namespace konbini::adapters::pictor {

// 1 つの `render::WorldMesh` に対応する vertex / index buffer の所有者。
//
// pin 済み Pictor の `VertexDataUploader` は staging allocation 後の copy が
// 未実装で、`register_mesh_data()` だけでは描画できない。first playable は
// その host-driven upload 責務を game 側のこの owner で補い、HOST_VISIBLE |
// HOST_COHERENT メモリへ直接書く。staging + transfer queue へ移すときも
// 境界はこのクラスのままにする。
//
// 借用した `VkDevice` はこの owner より長生きしなければならない。`shutdown()`
// は `VulkanContext::shutdown()` より前に呼ぶ必要があり、破棄は確保と逆順
// (index buffer → vertex buffer) に行う。
class WorldGeometryBuffer {
public:
    WorldGeometryBuffer() = default;
    ~WorldGeometryBuffer();

    WorldGeometryBuffer(const WorldGeometryBuffer&) = delete;
    WorldGeometryBuffer& operator=(const WorldGeometryBuffer&) = delete;

    // capacity は 0 を許さない。空 mesh 用の 0 byte buffer は Vulkan では
    // 生成できず、「描くものが無い」状態は capacity ではなく indexCount 0 で
    // 表現する。
    void initialize(
        VkPhysicalDevice physicalDevice, VkDevice device,
        VkDeviceSize vertexCapacityBytes, VkDeviceSize indexCapacityBytes);
    void shutdown() noexcept;

    // capacity を超える mesh は切り詰めず `std::length_error`。範囲外 index を
    // 含む mesh も GPU へ渡さず `std::out_of_range`。
    void upload(const render::WorldMesh& mesh);

    [[nodiscard]] bool isInitialized() const noexcept;
    [[nodiscard]] VkDeviceSize vertexCapacityBytes() const noexcept;
    [[nodiscard]] VkDeviceSize indexCapacityBytes() const noexcept;
    [[nodiscard]] std::uint32_t indexCount() const noexcept;

    // 未初期化 / 未 upload の handle を無言で VK_NULL_HANDLE として返さない。
    [[nodiscard]] VkBuffer vertexBuffer() const;
    [[nodiscard]] VkBuffer indexBuffer() const;

    [[nodiscard]] static VkDeviceSize requiredVertexBytes(
        const render::WorldMesh& mesh);
    [[nodiscard]] static VkDeviceSize requiredIndexBytes(
        const render::WorldMesh& mesh);

private:
    struct MappedBuffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        void* mapped = nullptr;
        VkDeviceSize capacityBytes = 0;
    };

    void destroy(MappedBuffer& target) noexcept;

    VkDevice device_ = VK_NULL_HANDLE;
    MappedBuffer vertices_;
    MappedBuffer indices_;
    std::uint32_t indexCount_ = 0;
};

}  // namespace konbini::adapters::pictor
