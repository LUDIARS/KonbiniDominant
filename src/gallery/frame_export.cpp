#include "frame_export.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <vector>
#include "konbini/adapters/pictor/world_scene_targets.h"
#include "pictor/surface/vulkan_context.h"

namespace konbini::gallery {
namespace {
void requireVk(VkResult result) {
    if (result != VK_SUCCESS) throw std::runtime_error("gallery GPU readback failed");
}
struct Readback {
    VkDevice device;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void* mapped = nullptr;
    ~Readback() {
        if (mapped) vkUnmapMemory(device, memory);
        if (buffer) vkDestroyBuffer(device, buffer, nullptr);
        if (memory) vkFreeMemory(device, memory, nullptr);
    }
};
// IEEE binary16 input, linear HDR -> IEC sRGB byte for the exported image.
unsigned char srgbByte(std::uint16_t half) {
    const int exponent = (half >> 10U) & 31U;
    const int mantissa = half & 1023U;
    float value = exponent == 0 ? std::ldexp(static_cast<float>(mantissa), -24)
                               : std::ldexp(1.0F + mantissa / 1024.0F, exponent - 15);
    if ((half & 32768U) != 0) value = -value;
    value = std::clamp(value, 0.0F, 1.0F);
    const float encoded = value <= 0.0031308F ? value * 12.92F
                                             : 1.055F * std::pow(value, 1.0F / 2.4F) - 0.055F;
    return static_cast<unsigned char>(std::lround(encoded * 255.0F));
}
}

void exportFrame(::pictor::VulkanContext& context,
                 const adapters::pictor::WorldSceneTargets& targets,
                 const std::filesystem::path& path) {
    if (!context.is_initialized() || !targets.isInitialized()) {
        throw std::logic_error("gallery capture requires live render targets");
    }
    context.device_wait_idle();
    const auto extent = targets.extent();
    const VkDeviceSize bytes = static_cast<VkDeviceSize>(extent.width) * extent.height * 8U;
    Readback staging{context.device()};
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = bytes;
    info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    requireVk(vkCreateBuffer(staging.device, &info, nullptr, &staging.buffer));
    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(staging.device, staging.buffer, &requirements);
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(context.physical_device(), &properties);
    std::uint32_t type = properties.memoryTypeCount;
    constexpr VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (std::uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
        if ((requirements.memoryTypeBits & (1U << i)) != 0 &&
            (properties.memoryTypes[i].propertyFlags & flags) == flags) { type = i; break; }
    }
    if (type == properties.memoryTypeCount) throw std::runtime_error("no coherent readback memory");
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = type;
    requireVk(vkAllocateMemory(staging.device, &allocation, nullptr, &staging.memory));
    requireVk(vkBindBufferMemory(staging.device, staging.buffer, staging.memory, 0));
    const auto flight = (context.current_frame() + targets.flightCount() - 1U) % targets.flightCount();
    const VkImage image = targets.colorImage(flight);
    const auto command = context.begin_single_time_commands();
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {extent.width, extent.height, 1};
    vkCmdCopyImageToBuffer(command, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           staging.buffer, 1, &copy);
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    context.end_single_time_commands(command);
    requireVk(vkMapMemory(staging.device, staging.memory, 0, bytes, 0, &staging.mapped));
    const auto* pixels = static_cast<const std::uint16_t*>(staging.mapped);
    std::vector<unsigned char> rgb(static_cast<std::size_t>(extent.width) * extent.height * 3U);
    for (std::size_t pixel = 0; pixel < rgb.size() / 3U; ++pixel) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            rgb[pixel * 3U + channel] = srgbByte(pixels[pixel * 4U + channel]);
        }
    }
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << "P6\n" << extent.width << ' ' << extent.height << "\n255\n";
    output.write(reinterpret_cast<const char*>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
    output.flush();
    if (!output) throw std::runtime_error("failed to write gallery image");
}
}
