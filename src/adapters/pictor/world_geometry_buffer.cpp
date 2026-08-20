#include "konbini/adapters/pictor/world_geometry_buffer.h"

#include <cstddef>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

// @implements spec/interface/pictor-rendering.md Required bridge

namespace konbini::adapters::pictor {

namespace {

[[noreturn]] void failVulkan(const char* operation, const VkResult result) {
    throw std::runtime_error(
        std::string(operation) + " failed with VkResult " +
        std::to_string(static_cast<int>(result)));
}

// CPU から毎フレーム書ける HOST_VISIBLE | HOST_COHERENT のみを対象にする。
// COHERENT を必須にしているのは、非 coherent メモリの明示 flush を省くため
// ではなく、flush 範囲を nonCoherentAtomSize へ丸める責務をこの owner へ
// 持ち込まないため。該当が無い device は fallback せず失敗させる。
[[nodiscard]] std::uint32_t findHostVisibleMemoryType(
    const VkPhysicalDevice physicalDevice,
    const std::uint32_t typeBits) {
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &properties);
    constexpr VkMemoryPropertyFlags kRequired =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (std::uint32_t index = 0; index < properties.memoryTypeCount;
         ++index) {
        if ((typeBits & (1U << index)) != 0U &&
            (properties.memoryTypes[index].propertyFlags & kRequired) ==
                kRequired) {
            return index;
        }
    }
    throw std::runtime_error(
        "no host-visible coherent memory type for world geometry");
}

}  // namespace

WorldGeometryBuffer::~WorldGeometryBuffer() {
    shutdown();
}

// @implements spec/interface/pictor-rendering.md Required bridge
void WorldGeometryBuffer::initialize(
    const VkPhysicalDevice physicalDevice, const VkDevice device,
    const VkDeviceSize vertexCapacityBytes,
    const VkDeviceSize indexCapacityBytes) {
    if (isInitialized()) {
        throw std::logic_error("world geometry buffer is already initialized");
    }
    if (physicalDevice == VK_NULL_HANDLE || device == VK_NULL_HANDLE ||
        vertexCapacityBytes == 0 || indexCapacityBytes == 0) {
        throw std::invalid_argument(
            "world geometry buffer requires a device and non-zero capacity");
    }

    device_ = device;
    // 確保順は vertex → index。異常時も含め破棄はこの逆順を守る。
    const struct {
        MappedBuffer* target;
        VkDeviceSize capacity;
        VkBufferUsageFlags usage;
    } requests[2] = {
        {&vertices_, vertexCapacityBytes,
         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT},
        {&indices_, indexCapacityBytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT},
    };

    try {
        for (const auto& request : requests) {
            MappedBuffer& target = *request.target;
            const VkBufferCreateInfo bufferInfo{
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = request.capacity,
                .usage = request.usage,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            };
            VkResult result = vkCreateBuffer(
                device_, &bufferInfo, nullptr, &target.buffer);
            if (result != VK_SUCCESS) {
                failVulkan("vkCreateBuffer", result);
            }

            VkMemoryRequirements requirements{};
            vkGetBufferMemoryRequirements(
                device_, target.buffer, &requirements);
            const VkMemoryAllocateInfo allocateInfo{
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = requirements.size,
                .memoryTypeIndex = findHostVisibleMemoryType(
                    physicalDevice, requirements.memoryTypeBits),
            };
            result = vkAllocateMemory(
                device_, &allocateInfo, nullptr, &target.memory);
            if (result != VK_SUCCESS) {
                failVulkan("vkAllocateMemory", result);
            }
            result =
                vkBindBufferMemory(device_, target.buffer, target.memory, 0);
            if (result != VK_SUCCESS) {
                failVulkan("vkBindBufferMemory", result);
            }
            // 毎フレーム書き換える buffer なので map/unmap を往復させず、
            // 生存期間中は map したままにする。
            result = vkMapMemory(
                device_, target.memory, 0, VK_WHOLE_SIZE, 0, &target.mapped);
            if (result != VK_SUCCESS) {
                failVulkan("vkMapMemory", result);
            }
            target.capacityBytes = request.capacity;
        }
    } catch (...) {
        shutdown();
        throw;
    }
}

void WorldGeometryBuffer::shutdown() noexcept {
    // 確保は vertex → index なので、破棄は index → vertex。
    destroy(indices_);
    destroy(vertices_);
    indexCount_ = 0;
    device_ = VK_NULL_HANDLE;
}

void WorldGeometryBuffer::destroy(MappedBuffer& target) noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    if (target.mapped != nullptr) {
        vkUnmapMemory(device_, target.memory);
        target.mapped = nullptr;
    }
    // Bound memory cannot be freed while the buffer that references it is
    // still alive. Destroy the resource first, then release its allocation.
    if (target.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, target.buffer, nullptr);
        target.buffer = VK_NULL_HANDLE;
    }
    if (target.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device_, target.memory, nullptr);
        target.memory = VK_NULL_HANDLE;
    }
    target.capacityBytes = 0;
}

// @implements spec/interface/pictor-rendering.md Required bridge
void WorldGeometryBuffer::upload(const render::WorldMesh& mesh) {
    if (!isInitialized()) {
        throw std::logic_error(
            "world geometry upload requires an initialized buffer");
    }
    if (mesh.vertices.empty() != mesh.indices.empty()) {
        throw std::invalid_argument(
            "world geometry mesh has vertices without indices or vice versa");
    }
    if (mesh.indices.size() % 3U != 0U) {
        throw std::invalid_argument(
            "world geometry mesh is not a triangle list");
    }
    if (mesh.vertices.size() >
        static_cast<std::size_t>(
            std::numeric_limits<std::uint32_t>::max())) {
        throw std::overflow_error("world geometry vertex count overflows");
    }
    if (mesh.indices.size() >
        static_cast<std::size_t>(
            std::numeric_limits<std::uint32_t>::max())) {
        throw std::overflow_error("world geometry index count overflows");
    }

    const VkDeviceSize vertexBytes = requiredVertexBytes(mesh);
    const VkDeviceSize indexBytes = requiredIndexBytes(mesh);
    if (vertexBytes > vertices_.capacityBytes ||
        indexBytes > indices_.capacityBytes) {
        throw std::length_error(
            "world geometry mesh exceeds the allocated buffer capacity");
    }

    const auto vertexLimit =
        static_cast<std::uint32_t>(mesh.vertices.size());
    for (const std::uint32_t index : mesh.indices) {
        if (index >= vertexLimit) {
            throw std::out_of_range(
                "world geometry index is outside the vertex range");
        }
    }

    if (vertexBytes != 0) {
        std::memcpy(
            vertices_.mapped, mesh.vertices.data(),
            static_cast<std::size_t>(vertexBytes));
        std::memcpy(
            indices_.mapped, mesh.indices.data(),
            static_cast<std::size_t>(indexBytes));
    }
    indexCount_ = static_cast<std::uint32_t>(mesh.indices.size());
}

bool WorldGeometryBuffer::isInitialized() const noexcept {
    return device_ != VK_NULL_HANDLE && vertices_.mapped != nullptr &&
           indices_.mapped != nullptr;
}

VkDeviceSize WorldGeometryBuffer::vertexCapacityBytes() const noexcept {
    return vertices_.capacityBytes;
}

VkDeviceSize WorldGeometryBuffer::indexCapacityBytes() const noexcept {
    return indices_.capacityBytes;
}

std::uint32_t WorldGeometryBuffer::indexCount() const noexcept {
    return indexCount_;
}

VkBuffer WorldGeometryBuffer::vertexBuffer() const {
    if (!isInitialized()) {
        throw std::logic_error(
            "world geometry vertex buffer requested before initialization");
    }
    return vertices_.buffer;
}

VkBuffer WorldGeometryBuffer::indexBuffer() const {
    if (!isInitialized()) {
        throw std::logic_error(
            "world geometry index buffer requested before initialization");
    }
    return indices_.buffer;
}

VkDeviceSize WorldGeometryBuffer::requiredVertexBytes(
    const render::WorldMesh& mesh) {
    return static_cast<VkDeviceSize>(mesh.vertices.size()) *
           sizeof(render::WorldVertex);
}

VkDeviceSize WorldGeometryBuffer::requiredIndexBytes(
    const render::WorldMesh& mesh) {
    return static_cast<VkDeviceSize>(mesh.indices.size()) *
           sizeof(std::uint32_t);
}

}  // namespace konbini::adapters::pictor
