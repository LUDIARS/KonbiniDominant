#include "konbini/adapters/pictor/world_scene_targets.h"

#include <array>
#include <cstddef>
#include <exception>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

#include "pictor/pipeline/attachment_def.h"
#include "pictor/pipeline/attachment_registry.h"
#include "pictor/pipeline/framebuffer_registry.h"
#include "pictor/pipeline/pipeline_profile.h"
#include "pictor/pipeline/render_pass_registry.h"
#include "pictor/surface/vulkan_context.h"

namespace konbini::adapters::pictor {

namespace {

constexpr char kColorAttachmentName[] = "scene_hdr_color";
constexpr char kDepthAttachmentName[] = "scene_depth";
constexpr char kWorldPassName[] = "konbini_world_scene";
constexpr std::uint32_t kMinimumFlightCount = 1;

// The attachment definition and the begin-render-pass clear values must stay
// identical; keep the single source here rather than repeating the literals.
constexpr float kSceneClearColorR = 0.025F;
constexpr float kSceneClearColorG = 0.035F;
constexpr float kSceneClearColorB = 0.055F;
constexpr float kSceneClearColorA = 1.0F;
constexpr float kSceneClearDepth = 1.0F;
constexpr std::uint32_t kSceneClearStencil = 0;

[[nodiscard]] bool isValidExtent(const VkExtent2D extent) noexcept {
    return extent.width > 0 && extent.height > 0;
}

[[nodiscard]] bool extentsMatch(
    const VkExtent2D left, const VkExtent2D right) noexcept {
    return left.width == right.width && left.height == right.height;
}

[[nodiscard]] std::uint32_t swapchainImageCount(
    const ::pictor::VulkanContext& context) {
    const std::size_t count = context.swapchain_image_views().size();
    if (count == 0 ||
        count >
            ::pictor::AttachmentRegistry::MAX_SWAPCHAIN_IMAGES) {
        throw std::runtime_error(
            "world scene target swapchain-image count must be within "
            "Pictor limits");
    }
    return static_cast<std::uint32_t>(count);
}

[[nodiscard]] std::array<::pictor::AttachmentDef, 2>
makeAttachmentDefinitions() {
    ::pictor::AttachmentDef color;
    color.name = kColorAttachmentName;
    color.kind = ::pictor::AttachmentKind::COLOR;
    color.format =
        ::pictor::AttachmentFormat::R16G16B16A16_SFLOAT;
    color.sizing = ::pictor::AttachmentSizing::SWAPCHAIN_RELATIVE;
    color.scale = 1.0F;
    color.usage = ::pictor::USAGE_COLOR_ATTACHMENT |
                  ::pictor::USAGE_SAMPLED | ::pictor::USAGE_TRANSFER_SRC;
    color.clear_color = {
        kSceneClearColorR, kSceneClearColorG, kSceneClearColorB,
        kSceneClearColorA};

    ::pictor::AttachmentDef depth;
    depth.name = kDepthAttachmentName;
    depth.kind = ::pictor::AttachmentKind::DEPTH;
    depth.format = ::pictor::AttachmentFormat::D32_SFLOAT;
    depth.sizing = ::pictor::AttachmentSizing::SWAPCHAIN_RELATIVE;
    depth.scale = 1.0F;
    depth.usage = ::pictor::USAGE_DEPTH_STENCIL_ATTACHMENT;
    depth.clear_depth = kSceneClearDepth;
    depth.clear_stencil = kSceneClearStencil;

    return {std::move(color), std::move(depth)};
}

[[nodiscard]] ::pictor::RenderPassDef makeWorldPassDefinition() {
    ::pictor::AttachmentOpsDef colorOps;
    colorOps.attachment_name = kColorAttachmentName;
    colorOps.load_op = ::pictor::AttachmentLoadOp::CLEAR;
    colorOps.store_op = ::pictor::AttachmentStoreOp::STORE;
    colorOps.initial_layout = ::pictor::ImageLayout::UNDEFINED;
    colorOps.final_layout =
        ::pictor::ImageLayout::SHADER_READ_ONLY_OPTIMAL;

    ::pictor::AttachmentOpsDef depthOps;
    depthOps.attachment_name = kDepthAttachmentName;
    depthOps.load_op = ::pictor::AttachmentLoadOp::CLEAR;
    depthOps.store_op = ::pictor::AttachmentStoreOp::DONT_CARE;
    depthOps.initial_layout = ::pictor::ImageLayout::UNDEFINED;
    depthOps.final_layout =
        ::pictor::ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    ::pictor::RenderPassDef pass;
    pass.pass_name = kWorldPassName;
    pass.pass_type = ::pictor::PassType::OPAQUE;
    pass.render_targets = {kColorAttachmentName, kDepthAttachmentName};
    pass.sort_mode = ::pictor::SortMode::FRONT_TO_BACK;
    pass.filter_mask = 0xFFFF;
    pass.gpu_driven_pass = false;
    pass.attachment_ops = {std::move(colorOps), std::move(depthOps)};
    pass.host_recorded = false;
    return pass;
}

[[nodiscard]] std::runtime_error resourceFailure(
    const char* operation) {
    return std::runtime_error(
        std::string("world scene targets failed to ") + operation);
}

}  // namespace

struct WorldSceneTargets::Impl {
    ::pictor::VulkanContext* context = nullptr;
    ::pictor::AttachmentRegistry attachments;
    ::pictor::RenderPassRegistry renderPasses;
    ::pictor::FramebufferRegistry framebuffers;
    VkDevice device = VK_NULL_HANDLE;
    VkExtent2D targetExtent{0, 0};
    std::uint32_t flights = 0;
    std::uint16_t colorIndex =
        ::pictor::AttachmentRegistry::INVALID_INDEX;
    std::uint16_t depthIndex =
        ::pictor::AttachmentRegistry::INVALID_INDEX;
    std::uint16_t passIndex =
        ::pictor::RenderPassRegistry::INVALID_INDEX;
    std::uint64_t generation = 0;
    bool initialized = false;

    void releaseVulkan() noexcept {
        framebuffers.shutdown_vulkan();
        renderPasses.shutdown_vulkan();
        attachments.shutdown_vulkan();
    }

    void validateResources(const VkExtent2D expectedExtent) const {
        if (attachments.flight_count() != flights ||
            attachments.slot_count(colorIndex) != flights ||
            attachments.slot_count(depthIndex) != flights ||
            attachments.vk_format(colorIndex) !=
                VK_FORMAT_R16G16B16A16_SFLOAT ||
            attachments.vk_format(depthIndex) != VK_FORMAT_D32_SFLOAT ||
            !extentsMatch(
                attachments.extent(colorIndex), expectedExtent) ||
            !extentsMatch(
                attachments.extent(depthIndex), expectedExtent) ||
            renderPasses.get(passIndex) == VK_NULL_HANDLE ||
            framebuffers.is_swapchain_pass(passIndex)) {
            throw resourceFailure("validate registry resources");
        }
        for (std::uint32_t flight = 0; flight < flights; ++flight) {
            if (attachments.image(colorIndex, flight) ==
                    VK_NULL_HANDLE ||
                attachments.view(colorIndex, flight) ==
                    VK_NULL_HANDLE ||
                attachments.image(depthIndex, flight) ==
                    VK_NULL_HANDLE ||
                attachments.view(depthIndex, flight) ==
                    VK_NULL_HANDLE ||
                framebuffers.get(passIndex, flight) ==
                    VK_NULL_HANDLE) {
                throw resourceFailure("validate per-flight resources");
            }
        }
    }

    void resetState() noexcept {
        context = nullptr;
        device = VK_NULL_HANDLE;
        targetExtent = {0, 0};
        flights = 0;
        colorIndex = ::pictor::AttachmentRegistry::INVALID_INDEX;
        depthIndex = ::pictor::AttachmentRegistry::INVALID_INDEX;
        passIndex = ::pictor::RenderPassRegistry::INVALID_INDEX;
        // `generation` is deliberately not reset: it must never repeat a
        // value for this object, or a borrower that cached the generation
        // across a shutdown/initialize cycle would compare equal.
        initialized = false;
    }
};

WorldSceneTargets::WorldSceneTargets()
    : impl_(std::make_unique<Impl>()) {}

WorldSceneTargets::~WorldSceneTargets() {
    shutdown();
}

// @implements spec/interface/pictor-rendering.md Offscreen world composition
void WorldSceneTargets::initialize(
    ::pictor::VulkanContext& context) {
    if (impl_->initialized) {
        throw std::logic_error(
            "world scene targets are already initialized");
    }
    // Query the context only after it reports itself initialized; the
    // swapchain/flight accessors are not contractually safe before that.
    if (!context.is_initialized() ||
        context.physical_device() == VK_NULL_HANDLE ||
        context.device() == VK_NULL_HANDLE) {
        throw std::invalid_argument(
            "world scene targets require an initialized Vulkan context");
    }
    const VkExtent2D initialExtent = context.swapchain_extent();
    const std::uint32_t flights = context.frames_in_flight();
    if (!isValidExtent(initialExtent)) {
        throw std::invalid_argument(
            "world scene targets require a non-zero swapchain extent");
    }
    if (flights < kMinimumFlightCount ||
        flights > ::pictor::AttachmentRegistry::MAX_FLIGHTS) {
        throw std::invalid_argument(
            "world scene target flight count must be within Pictor limits");
    }
    const std::uint32_t swapchainImages =
        swapchainImageCount(context);

    const auto attachments = makeAttachmentDefinitions();
    const std::array passes = {makeWorldPassDefinition()};
    impl_->attachments.set_defs(
        std::span<const ::pictor::AttachmentDef>(attachments));
    impl_->renderPasses.set_passes(
        std::span<const ::pictor::RenderPassDef>(passes));
    impl_->colorIndex =
        impl_->attachments.index_of(kColorAttachmentName);
    impl_->depthIndex =
        impl_->attachments.index_of(kDepthAttachmentName);
    impl_->passIndex =
        impl_->renderPasses.index_of(kWorldPassName);
    if (impl_->colorIndex ==
            ::pictor::AttachmentRegistry::INVALID_INDEX ||
        impl_->depthIndex ==
            ::pictor::AttachmentRegistry::INVALID_INDEX ||
        impl_->passIndex ==
            ::pictor::RenderPassRegistry::INVALID_INDEX) {
        impl_->resetState();
        throw std::logic_error(
            "world scene target registry definitions are inconsistent");
    }

    try {
        if (!impl_->attachments.initialize_vulkan(
                context.physical_device(), context.device(),
                initialExtent, flights)) {
            throw resourceFailure("allocate attachments");
        }
        if (!impl_->renderPasses.initialize_vulkan(
                context.device(), impl_->attachments)) {
            throw resourceFailure("create the render pass");
        }
        if (!impl_->framebuffers.initialize_vulkan(
                context.device(), impl_->attachments,
                impl_->renderPasses, flights, swapchainImages)) {
            throw resourceFailure("create framebuffers");
        }

        impl_->flights = flights;
        impl_->validateResources(initialExtent);
    } catch (...) {
        impl_->releaseVulkan();
        impl_->resetState();
        throw;
    }

    impl_->context = &context;
    impl_->device = context.device();
    impl_->targetExtent = initialExtent;
    ++impl_->generation;
    impl_->initialized = true;
}

// @implements spec/interface/pictor-rendering.md Offscreen world composition
void WorldSceneTargets::resize(const VkExtent2D extent) {
    if (!impl_->initialized || impl_->context == nullptr) {
        throw std::logic_error(
            "world scene target resize requires initialization");
    }
    if (!isValidExtent(extent)) {
        throw std::invalid_argument(
            "world scene target extent must be non-zero");
    }
    if (!impl_->context->is_initialized() ||
        impl_->context->device() != impl_->device ||
        impl_->context->frames_in_flight() != impl_->flights) {
        throw std::runtime_error(
            "world scene target Vulkan context changed during resize");
    }
    if (!extentsMatch(
            extent, impl_->context->swapchain_extent())) {
        throw std::invalid_argument(
            "world scene target extent must match the Vulkan swapchain");
    }
    if (extentsMatch(extent, impl_->targetExtent)) {
        return;
    }

    impl_->context->device_wait_idle();
    WorldSceneTargets replacement;
    replacement.initialize(*impl_->context);
    if (!extentsMatch(replacement.extent(), extent)) {
        throw std::runtime_error(
            "replacement world scene targets have a stale extent");
    }
    const std::uint64_t retiredGeneration = impl_->generation;
    impl_.swap(replacement.impl_);
    // The replacement counted from its own zero; continue this object's
    // sequence instead so borrowers observe the bundle change.
    impl_->generation = retiredGeneration + 1;
}

// @implements spec/interface/pictor-rendering.md Offscreen world composition
void WorldSceneTargets::shutdown() noexcept {
    if (impl_ == nullptr || !impl_->initialized) {
        return;
    }
    if (impl_->context == nullptr ||
        !impl_->context->is_initialized() ||
        impl_->context->device() != impl_->device) {
        // Pictor registries retain their VkDevice and cannot abandon handles.
        // The required lifetime is targets -> VulkanContext; violating it must
        // fail before issuing vkDestroy* calls against a dead device.
        std::terminate();
    }
    impl_->context->device_wait_idle();
    impl_->releaseVulkan();
    impl_->resetState();
}

bool WorldSceneTargets::isInitialized() const noexcept {
    return impl_ != nullptr && impl_->initialized;
}

// @implements spec/interface/pictor-rendering.md Offscreen world composition
std::uint64_t WorldSceneTargets::generation() const noexcept {
    return impl_ != nullptr && impl_->initialized ? impl_->generation : 0;
}

std::uint32_t WorldSceneTargets::flightCount() const noexcept {
    return impl_ != nullptr ? impl_->flights : 0;
}

VkExtent2D WorldSceneTargets::extent() const noexcept {
    return impl_ != nullptr ? impl_->targetExtent : VkExtent2D{0, 0};
}

VkRenderPass WorldSceneTargets::renderPass() const noexcept {
    if (impl_ == nullptr || !impl_->initialized) {
        return VK_NULL_HANDLE;
    }
    return impl_->renderPasses.get(impl_->passIndex);
}

VkFramebuffer WorldSceneTargets::framebuffer(
    const std::uint32_t flightIndex) const {
    if (!impl_->initialized) {
        throw std::logic_error(
            "world scene framebuffer requested before initialization");
    }
    if (flightIndex >= impl_->flights) {
        throw std::out_of_range(
            "world scene framebuffer flight is out of range");
    }
    const VkFramebuffer result =
        impl_->framebuffers.get(impl_->passIndex, flightIndex);
    if (result == VK_NULL_HANDLE) {
        throw resourceFailure("resolve a framebuffer");
    }
    return result;
}

VkFramebuffer WorldSceneTargets::currentFramebuffer() const {
    return framebuffer(currentFlight());
}

VkImageView WorldSceneTargets::colorView(
    const std::uint32_t flightIndex) const {
    if (!impl_->initialized) {
        throw std::logic_error(
            "world scene color view requested before initialization");
    }
    if (flightIndex >= impl_->flights) {
        throw std::out_of_range(
            "world scene color-view flight is out of range");
    }
    const VkImageView result =
        impl_->attachments.view(impl_->colorIndex, flightIndex);
    if (result == VK_NULL_HANDLE) {
        throw resourceFailure("resolve a color view");
    }
    return result;
}

VkImageView WorldSceneTargets::currentColorView() const {
    return colorView(currentFlight());
}

VkImageView WorldSceneTargets::depthView(
    const std::uint32_t flightIndex) const {
    if (!impl_->initialized) {
        throw std::logic_error(
            "world scene depth view requested before initialization");
    }
    if (flightIndex >= impl_->flights) {
        throw std::out_of_range(
            "world scene depth-view flight is out of range");
    }
    const VkImageView result =
        impl_->attachments.view(impl_->depthIndex, flightIndex);
    if (result == VK_NULL_HANDLE) {
        throw resourceFailure("resolve a depth view");
    }
    return result;
}

VkImageView WorldSceneTargets::currentDepthView() const {
    return depthView(currentFlight());
}

std::array<VkClearValue, 2> WorldSceneTargets::clearValues() noexcept {
    std::array<VkClearValue, 2> values{};
    values[0].color.float32[0] = kSceneClearColorR;
    values[0].color.float32[1] = kSceneClearColorG;
    values[0].color.float32[2] = kSceneClearColorB;
    values[0].color.float32[3] = kSceneClearColorA;
    values[1].depthStencil.depth = kSceneClearDepth;
    values[1].depthStencil.stencil = kSceneClearStencil;
    return values;
}

// @implements spec/interface/pictor-rendering.md Offscreen world composition
void WorldSceneTargets::recordColorShaderReadBarrier(
    const VkCommandBuffer commandBuffer) const {
    if (commandBuffer == VK_NULL_HANDLE) {
        throw std::invalid_argument(
            "world scene color barrier requires a command buffer");
    }
    VkImageMemoryBarrier barrier{
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = colorImage(currentFlight());
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);
}

std::uint32_t WorldSceneTargets::currentFlight() const {
    if (!impl_->initialized || impl_->context == nullptr) {
        throw std::logic_error(
            "world scene current flight requested before initialization");
    }
    if (!impl_->context->is_initialized() ||
        impl_->context->device() != impl_->device ||
        impl_->context->frames_in_flight() != impl_->flights) {
        throw std::runtime_error(
            "world scene target Vulkan context is no longer compatible");
    }
    const std::uint32_t flight = impl_->context->current_frame();
    if (flight >= impl_->flights) {
        throw std::runtime_error(
            "Pictor current flight exceeds world scene target slots");
    }
    return flight;
}

VkImage WorldSceneTargets::colorImage(
    const std::uint32_t flightIndex) const {
    if (!impl_->initialized) {
        throw std::logic_error(
            "world scene color image requested before initialization");
    }
    if (flightIndex >= impl_->flights) {
        throw std::out_of_range(
            "world scene color-image flight is out of range");
    }
    const VkImage result =
        impl_->attachments.image(impl_->colorIndex, flightIndex);
    if (result == VK_NULL_HANDLE) {
        throw resourceFailure("resolve a color image");
    }
    return result;
}

}  // namespace konbini::adapters::pictor
