#pragma once

#include <array>
#include <cstdint>
#include <memory>

#ifndef NOGDI
#define NOGDI
#endif
#include <vulkan/vulkan.h>

namespace pictor {
class VulkanContext;
}

namespace konbini::adapters::pictor {

// @implements spec/interface/pictor-rendering.md Offscreen world composition
class WorldSceneTargets {
public:
    // Borrowed VulkanContext must outlive this owner. `shutdown()` must run
    // before VulkanContext::shutdown(); violating the order is fatal because
    // Pictor registries cannot abandon raw VkDevice-owned handles.
    WorldSceneTargets();
    ~WorldSceneTargets();

    WorldSceneTargets(const WorldSceneTargets&) = delete;
    WorldSceneTargets& operator=(const WorldSceneTargets&) = delete;

    // The borrowed VulkanContext must outlive this owner. Call shutdown()
    // before VulkanContext::shutdown(); violating that order is fatal because
    // Pictor's registries retain the device used to destroy their resources.
    void initialize(::pictor::VulkanContext& context);

    // The caller must destroy every descriptor/pipeline that references the
    // current views before resize. A replacement bundle is installed only
    // after all new resources succeed; failure leaves the old bundle owned.
    void resize(VkExtent2D extent);
    void shutdown() noexcept;

    [[nodiscard]] bool isInitialized() const noexcept;

    // Identifies the currently owned image/view bundle. Every successful
    // initialize() or resize() publishes a new value, so a borrower that
    // caches views (descriptors, framebuffers) can fail fast instead of
    // sampling handles destroyed by the previous bundle. 0 means uninitialized.
    [[nodiscard]] std::uint64_t generation() const noexcept;

    [[nodiscard]] std::uint32_t flightCount() const noexcept;
    [[nodiscard]] VkExtent2D extent() const noexcept;
    [[nodiscard]] VkRenderPass renderPass() const noexcept;

    [[nodiscard]] VkFramebuffer framebuffer(
        std::uint32_t flightIndex) const;
    [[nodiscard]] VkFramebuffer currentFramebuffer() const;
    [[nodiscard]] VkImageView colorView(
        std::uint32_t flightIndex) const;
    [[nodiscard]] VkImageView currentColorView() const;
    [[nodiscard]] VkImageView depthView(
        std::uint32_t flightIndex) const;
    [[nodiscard]] VkImageView currentDepthView() const;

    [[nodiscard]] static std::array<VkClearValue, 2> clearValues()
        noexcept;

    void recordColorShaderReadBarrier(
        VkCommandBuffer commandBuffer) const;

private:
    struct Impl;

    [[nodiscard]] std::uint32_t currentFlight() const;
    [[nodiscard]] VkImage colorImage(
        std::uint32_t flightIndex) const;

    std::unique_ptr<Impl> impl_;
};

}  // namespace konbini::adapters::pictor
