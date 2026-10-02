#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "konbini/adapters/pictor/pictor_batch_plan.h"

#ifndef NOGDI
#define NOGDI
#endif
#include <vulkan/vulkan.h>

// @implements spec/interface/pictor-rendering.md Required bridge

namespace konbini::adapters::pictor {

// Per-flight storage buffers holding `WorldInstanceRecord`s, plus the
// descriptor set that exposes each one to `konbini_world_instanced.vert`
// (set 0, binding 0).
//
// Flight N may be written only after `VulkanContext::acquire_next_image()`
// waited flight N's fence, i.e. while recording `current_frame() == N`.
// Growing a flight's buffer recreates that buffer and rewrites that flight's
// descriptor; other flights, which may still be in flight, are untouched.
//
// The borrowed device must outlive this owner. Release order is buffers →
// descriptor pool → set layout, the reverse of creation.
class WorldInstanceBuffers {
public:
    WorldInstanceBuffers() = default;
    ~WorldInstanceBuffers();

    WorldInstanceBuffers(const WorldInstanceBuffers&) = delete;
    WorldInstanceBuffers& operator=(const WorldInstanceBuffers&) = delete;

    void initialize(VkPhysicalDevice physicalDevice, VkDevice device,
                    std::uint32_t flightCount);
    void shutdown() noexcept;

    [[nodiscard]] bool isInitialized() const noexcept;
    [[nodiscard]] std::uint32_t flightCount() const noexcept;
    [[nodiscard]] VkDescriptorSetLayout setLayout() const;

    // Writes `records` into the flight's buffer and returns its descriptor
    // set. An empty span still returns a valid set (nothing will be drawn).
    [[nodiscard]] VkDescriptorSet upload(
        std::uint32_t flightIndex, std::span<const WorldInstanceRecord> records);

private:
    struct FlightBuffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        void* mapped = nullptr;
        VkDeviceSize capacityBytes = 0;
        VkDescriptorSet set = VK_NULL_HANDLE;
    };

    void allocate(FlightBuffer& flight, VkDeviceSize capacityBytes);
    void destroyBuffer(FlightBuffer& flight) noexcept;
    void writeDescriptor(const FlightBuffer& flight) const;

    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout setLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool pool_ = VK_NULL_HANDLE;
    std::vector<FlightBuffer> flights_;
};

}  // namespace konbini::adapters::pictor
