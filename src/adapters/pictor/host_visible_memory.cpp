#include "konbini/adapters/pictor/host_visible_memory.h"

#include <stdexcept>

// @implements spec/interface/pictor-rendering.md Required bridge

namespace konbini::adapters::pictor {

std::uint32_t findHostVisibleCoherentMemoryType(
    const VkPhysicalDevice physicalDevice, const std::uint32_t typeBits) {
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

}  // namespace konbini::adapters::pictor
