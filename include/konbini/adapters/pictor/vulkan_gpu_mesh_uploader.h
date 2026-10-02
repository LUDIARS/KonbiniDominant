#pragma once

#include <memory>
#include <vector>

#include "konbini/adapters/pictor/gpu_mesh_uploader.h"
#include "konbini/adapters/pictor/world_geometry_buffer.h"

// @implements spec/interface/pictor-rendering.md Required bridge

namespace konbini::adapters::pictor {

// `IGpuMeshUploader` over `WorldGeometryBuffer`.
//
// Pinned Pictor's `VertexDataUploader` leaves the staging copy unimplemented,
// so the upload writes HOST_VISIBLE | HOST_COHERENT memory directly. The copy
// is complete when `upload()` returns, which is what lets `GpuAssetStore`
// publish a `MeshHandle` only for meshes that are already resident.
//
// The borrowed `VkDevice` must outlive this uploader. `shutdown()` releases
// every buffer still owned, newest first, and must run before the device is
// destroyed and after the GPU is idle.
class VulkanGpuMeshUploader final : public IGpuMeshUploader {
public:
    VulkanGpuMeshUploader() = default;
    ~VulkanGpuMeshUploader() override;

    VulkanGpuMeshUploader(const VulkanGpuMeshUploader&) = delete;
    VulkanGpuMeshUploader& operator=(const VulkanGpuMeshUploader&) = delete;

    void initialize(VkPhysicalDevice physicalDevice, VkDevice device);
    void shutdown() noexcept;

    [[nodiscard]] bool isInitialized() const noexcept;
    [[nodiscard]] std::size_t liveMeshCount() const noexcept;

    [[nodiscard]] GpuMeshBuffers upload(
        const render::WorldMesh& mesh) override;
    void release(const GpuMeshBuffers& buffers) noexcept override;

private:
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    // Allocation order. `release()` is rare (eviction / shutdown), so a
    // linear lookup by vertex buffer keeps reverse-order teardown trivial.
    std::vector<std::unique_ptr<WorldGeometryBuffer>> live_;
};

}  // namespace konbini::adapters::pictor
