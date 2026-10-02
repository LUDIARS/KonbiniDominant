#pragma once

#include <cstdint>

#include "konbini/render/world_mesh.h"

#ifndef NOGDI
#define NOGDI
#endif
#include <vulkan/vulkan.h>

// @implements spec/interface/pictor-rendering.md Required bridge

namespace konbini::adapters::pictor {

// One uploaded mesh as the GPU sees it. Pictor's `CompiledBatchRecorder`
// binds both buffers at offset 0 with `firstIndex = vertexOffset = 0`, so a
// mesh owns whole buffers instead of a range inside a shared one.
struct GpuMeshBuffers {
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    std::uint32_t indexCount = 0;
    VkDeviceSize residentBytes = 0;
};

// The seam between `GpuAssetStore` bookkeeping and the Vulkan allocation.
//
// `upload()` returns only after the data is visible to any command buffer
// submitted later; there is no "uploading" state for a caller to observe.
// Failure is an exception, never a null or partially filled result.
// `release()` requires that no pending submit still references the buffers.
class IGpuMeshUploader {
public:
    virtual ~IGpuMeshUploader() = default;

    [[nodiscard]] virtual GpuMeshBuffers upload(
        const render::WorldMesh& mesh) = 0;
    virtual void release(const GpuMeshBuffers& buffers) noexcept = 0;
};

}  // namespace konbini::adapters::pictor
