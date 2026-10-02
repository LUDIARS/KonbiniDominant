#include "konbini/adapters/pictor/vulkan_gpu_mesh_uploader.h"

#include <stdexcept>
#include <utility>

// @implements spec/interface/pictor-rendering.md Required bridge

namespace konbini::adapters::pictor {

VulkanGpuMeshUploader::~VulkanGpuMeshUploader() {
    shutdown();
}

void VulkanGpuMeshUploader::initialize(
    const VkPhysicalDevice physicalDevice, const VkDevice device) {
    if (isInitialized()) {
        throw std::logic_error("Vulkan mesh uploader is already initialized");
    }
    if (physicalDevice == VK_NULL_HANDLE || device == VK_NULL_HANDLE) {
        throw std::invalid_argument(
            "Vulkan mesh uploader requires a Vulkan device");
    }
    physicalDevice_ = physicalDevice;
    device_ = device;
}

void VulkanGpuMeshUploader::shutdown() noexcept {
    // The owning store normally releases everything first. Anything left is
    // destroyed newest first, matching allocation order in reverse.
    while (!live_.empty()) {
        live_.back()->shutdown();
        live_.pop_back();
    }
    physicalDevice_ = VK_NULL_HANDLE;
    device_ = VK_NULL_HANDLE;
}

bool VulkanGpuMeshUploader::isInitialized() const noexcept {
    return device_ != VK_NULL_HANDLE;
}

std::size_t VulkanGpuMeshUploader::liveMeshCount() const noexcept {
    return live_.size();
}

// @implements spec/interface/pictor-rendering.md Required bridge
GpuMeshBuffers VulkanGpuMeshUploader::upload(const render::WorldMesh& mesh) {
    if (!isInitialized()) {
        throw std::logic_error(
            "Vulkan mesh uploader used before initialization");
    }
    auto buffer = std::make_unique<WorldGeometryBuffer>();
    buffer->initialize(
        physicalDevice_, device_,
        WorldGeometryBuffer::requiredVertexBytes(mesh),
        WorldGeometryBuffer::requiredIndexBytes(mesh));
    buffer->upload(mesh);

    const GpuMeshBuffers result{
        .vertexBuffer = buffer->vertexBuffer(),
        .indexBuffer = buffer->indexBuffer(),
        .indexCount = buffer->indexCount(),
        .residentBytes =
            buffer->vertexCapacityBytes() + buffer->indexCapacityBytes(),
    };
    // A failed push_back destroys `buffer` through its destructor, so no
    // allocation escapes without an owner.
    live_.push_back(std::move(buffer));
    return result;
}

void VulkanGpuMeshUploader::release(const GpuMeshBuffers& buffers) noexcept {
    for (auto entry = live_.begin(); entry != live_.end(); ++entry) {
        if ((*entry)->vertexBuffer() == buffers.vertexBuffer) {
            (*entry)->shutdown();
            live_.erase(entry);
            return;
        }
    }
}

}  // namespace konbini::adapters::pictor
