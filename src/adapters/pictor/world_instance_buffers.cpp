#include "konbini/adapters/pictor/world_instance_buffers.h"

#include <cstring>
#include <stdexcept>
#include <string>

#include "konbini/adapters/pictor/host_visible_memory.h"

// @implements spec/interface/pictor-rendering.md Required bridge

namespace konbini::adapters::pictor {

namespace {

// Room for a few hundred facilities before the first growth.
constexpr VkDeviceSize kInitialCapacityBytes =
    256 * sizeof(WorldInstanceRecord);

[[noreturn]] void failVulkan(const char* operation, const VkResult result) {
    throw std::runtime_error(
        std::string(operation) + " failed with VkResult " +
        std::to_string(static_cast<int>(result)));
}

}  // namespace

WorldInstanceBuffers::~WorldInstanceBuffers() {
    shutdown();
}

void WorldInstanceBuffers::initialize(
    const VkPhysicalDevice physicalDevice, const VkDevice device,
    const std::uint32_t flightCount) {
    if (isInitialized()) {
        throw std::logic_error("world instance buffers are already initialized");
    }
    if (physicalDevice == VK_NULL_HANDLE || device == VK_NULL_HANDLE ||
        flightCount == 0) {
        throw std::invalid_argument(
            "world instance buffers require a device and a flight count");
    }
    physicalDevice_ = physicalDevice;
    device_ = device;
    try {
        const VkDescriptorSetLayoutBinding binding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        };
        const VkDescriptorSetLayoutCreateInfo layoutInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings = &binding,
        };
        VkResult result = vkCreateDescriptorSetLayout(
            device_, &layoutInfo, nullptr, &setLayout_);
        if (result != VK_SUCCESS) {
            failVulkan("vkCreateDescriptorSetLayout", result);
        }

        const VkDescriptorPoolSize poolSize{
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = flightCount,
        };
        const VkDescriptorPoolCreateInfo poolInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = flightCount,
            .poolSizeCount = 1,
            .pPoolSizes = &poolSize,
        };
        result = vkCreateDescriptorPool(device_, &poolInfo, nullptr, &pool_);
        if (result != VK_SUCCESS) {
            failVulkan("vkCreateDescriptorPool", result);
        }

        flights_.resize(flightCount);
        const std::vector<VkDescriptorSetLayout> layouts(flightCount, setLayout_);
        std::vector<VkDescriptorSet> sets(flightCount, VK_NULL_HANDLE);
        const VkDescriptorSetAllocateInfo allocateInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = pool_,
            .descriptorSetCount = flightCount,
            .pSetLayouts = layouts.data(),
        };
        result = vkAllocateDescriptorSets(device_, &allocateInfo, sets.data());
        if (result != VK_SUCCESS) {
            failVulkan("vkAllocateDescriptorSets", result);
        }
        for (std::uint32_t index = 0; index < flightCount; ++index) {
            flights_[index].set = sets[index];
            allocate(flights_[index], kInitialCapacityBytes);
            writeDescriptor(flights_[index]);
        }
    } catch (...) {
        shutdown();
        throw;
    }
}

void WorldInstanceBuffers::shutdown() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    for (auto flight = flights_.rbegin(); flight != flights_.rend(); ++flight) {
        destroyBuffer(*flight);
    }
    flights_.clear();
    // Destroying the pool frees its sets.
    if (pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, pool_, nullptr);
        pool_ = VK_NULL_HANDLE;
    }
    if (setLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, setLayout_, nullptr);
        setLayout_ = VK_NULL_HANDLE;
    }
    physicalDevice_ = VK_NULL_HANDLE;
    device_ = VK_NULL_HANDLE;
}

bool WorldInstanceBuffers::isInitialized() const noexcept {
    return device_ != VK_NULL_HANDLE && pool_ != VK_NULL_HANDLE &&
           !flights_.empty();
}

std::uint32_t WorldInstanceBuffers::flightCount() const noexcept {
    return static_cast<std::uint32_t>(flights_.size());
}

VkDescriptorSetLayout WorldInstanceBuffers::setLayout() const {
    if (setLayout_ == VK_NULL_HANDLE) {
        throw std::logic_error(
            "world instance set layout requested before initialization");
    }
    return setLayout_;
}

void WorldInstanceBuffers::allocate(
    FlightBuffer& flight, const VkDeviceSize capacityBytes) {
    const VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = capacityBytes,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    FlightBuffer next{.set = flight.set};
    try {
        VkResult result =
            vkCreateBuffer(device_, &bufferInfo, nullptr, &next.buffer);
        if (result != VK_SUCCESS) {
            failVulkan("vkCreateBuffer", result);
        }
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device_, next.buffer, &requirements);
        const VkMemoryAllocateInfo allocateInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = requirements.size,
            .memoryTypeIndex = findHostVisibleCoherentMemoryType(
                physicalDevice_, requirements.memoryTypeBits),
        };
        result = vkAllocateMemory(device_, &allocateInfo, nullptr, &next.memory);
        if (result != VK_SUCCESS) {
            failVulkan("vkAllocateMemory", result);
        }
        result = vkBindBufferMemory(device_, next.buffer, next.memory, 0);
        if (result != VK_SUCCESS) {
            failVulkan("vkBindBufferMemory", result);
        }
        result = vkMapMemory(
            device_, next.memory, 0, VK_WHOLE_SIZE, 0, &next.mapped);
        if (result != VK_SUCCESS) {
            failVulkan("vkMapMemory", result);
        }
        next.capacityBytes = capacityBytes;
    } catch (...) {
        destroyBuffer(next);
        throw;
    }
    // The new buffer exists before the old one goes, so a failed growth
    // leaves the flight's previous buffer and descriptor intact.
    destroyBuffer(flight);
    flight = next;
}

void WorldInstanceBuffers::destroyBuffer(FlightBuffer& flight) noexcept {
    if (flight.mapped != nullptr) {
        vkUnmapMemory(device_, flight.memory);
        flight.mapped = nullptr;
    }
    if (flight.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, flight.buffer, nullptr);
        flight.buffer = VK_NULL_HANDLE;
    }
    if (flight.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device_, flight.memory, nullptr);
        flight.memory = VK_NULL_HANDLE;
    }
    flight.capacityBytes = 0;
}

void WorldInstanceBuffers::writeDescriptor(const FlightBuffer& flight) const {
    const VkDescriptorBufferInfo bufferInfo{
        .buffer = flight.buffer,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };
    const VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = flight.set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &bufferInfo,
    };
    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
}

// @implements spec/interface/pictor-rendering.md Required bridge
VkDescriptorSet WorldInstanceBuffers::upload(
    const std::uint32_t flightIndex,
    const std::span<const WorldInstanceRecord> records) {
    if (!isInitialized()) {
        throw std::logic_error(
            "world instance buffers used before initialization");
    }
    if (flightIndex >= flights_.size()) {
        throw std::out_of_range(
            "world instance buffers flight index is out of range");
    }
    FlightBuffer& flight = flights_[flightIndex];
    const VkDeviceSize required =
        static_cast<VkDeviceSize>(records.size_bytes());
    if (required > flight.capacityBytes) {
        VkDeviceSize capacity = flight.capacityBytes;
        while (capacity < required) {
            capacity *= 2;
        }
        allocate(flight, capacity);
        writeDescriptor(flight);
    }
    if (required != 0) {
        std::memcpy(flight.mapped, records.data(),
                    static_cast<std::size_t>(required));
    }
    return flight.set;
}

}  // namespace konbini::adapters::pictor
