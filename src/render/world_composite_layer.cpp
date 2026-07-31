#include "konbini/render/world_composite_layer.h"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "ergo/render/render_context.h"
#include "konbini/adapters/pictor/world_scene_targets.h"
#include "pictor/shader/graphics_pipeline_builder.h"
#include "pictor/surface/vulkan_context.h"

namespace konbini::render {
namespace {

[[nodiscard]] bool isSrgbSwapchainFormat(
    const VkFormat format) noexcept {
    switch (format) {
        case VK_FORMAT_R8G8B8_SRGB:
        case VK_FORMAT_B8G8R8_SRGB:
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
            return true;
        default:
            return false;
    }
}

[[noreturn]] void failVulkan(
    const char* operation, const VkResult result) {
    throw std::runtime_error(
        std::string(operation) + " failed with VkResult " +
        std::to_string(static_cast<int>(result)));
}

std::vector<std::uint32_t> readSpirv(
    const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        throw std::runtime_error(
            "required SPIR-V shader is missing: " + path.string());
    }
    const std::streamoff byteCount = stream.tellg() - std::streampos(0);
    constexpr std::streamoff kMaxShaderBytes = 64 * 1024 * 1024;
    if (byteCount <= 0 || byteCount > kMaxShaderBytes ||
        byteCount % static_cast<std::streamoff>(
                        sizeof(std::uint32_t)) != 0) {
        throw std::runtime_error(
            "invalid SPIR-V shader size: " + path.string());
    }

    std::vector<std::uint32_t> words(
        static_cast<std::size_t>(byteCount) /
        sizeof(std::uint32_t));
    stream.seekg(0, std::ios::beg);
    stream.read(
        reinterpret_cast<char*>(words.data()),
        static_cast<std::streamsize>(byteCount));
    if (!stream) {
        throw std::runtime_error(
            "failed to read SPIR-V shader: " + path.string());
    }

    // `shader_dir` is runtime configuration, so the bytes are not trusted to
    // be SPIR-V. vkCreateShaderModule has no validation of its own and a
    // driver fed arbitrary bytes is undefined behaviour, not a clean error.
    constexpr std::uint32_t kSpirvMagic = 0x07230203U;
    if (words.front() != kSpirvMagic) {
        throw std::runtime_error(
            "file is not SPIR-V (bad magic word): " + path.string());
    }
    return words;
}

VkShaderModule createShaderModule(
    const VkDevice device,
    const std::vector<std::uint32_t>& words) {
    if (device == VK_NULL_HANDLE || words.empty()) {
        throw std::invalid_argument(
            "shader module requires a Vulkan device and SPIR-V");
    }

    const VkShaderModuleCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = words.size() * sizeof(std::uint32_t),
        .pCode = words.data(),
    };
    VkShaderModule module = VK_NULL_HANDLE;
    const VkResult result =
        vkCreateShaderModule(device, &info, nullptr, &module);
    if (result != VK_SUCCESS) {
        failVulkan("vkCreateShaderModule", result);
    }
    return module;
}

}  // namespace

struct WorldCompositeLayer::Impl {
    ::ergo::render::RenderContext* context = nullptr;
    VkDevice device = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    // Bundle the descriptor sets were written against. WorldSceneTargets
    // destroys the previous image views on resize, so binding a set from an
    // older bundle would sample freed handles.
    std::uint64_t targetsGeneration = 0;
    std::vector<std::uint32_t> vertexShader;
    std::vector<std::uint32_t> fragmentShader;
    bool initialized = false;
};

WorldCompositeLayer::WorldCompositeLayer(
    adapters::pictor::WorldSceneTargets& targets)
    : targets_(&targets), impl_(std::make_unique<Impl>()) {}

WorldCompositeLayer::~WorldCompositeLayer() {
    shutdown();
}

// @implements spec/interface/pictor-rendering.md Offscreen world composition
void WorldCompositeLayer::initialize(
    ::ergo::render::RenderContext& context) {
    if (impl_->context != nullptr || impl_->initialized) {
        throw std::logic_error(
            "world composite layer is already initialized");
    }
    if (context.vk == nullptr || !context.vk->is_initialized() ||
        context.vk->device() == VK_NULL_HANDLE ||
        context.vk->default_render_pass() == VK_NULL_HANDLE ||
        context.shader_dir.empty() || targets_ == nullptr ||
        !targets_->isInitialized()) {
        throw std::invalid_argument(
            "world composite layer initialization prerequisites failed");
    }

    const std::uint32_t flightCount = context.vk->frames_in_flight();
    const VkExtent2D swapchainExtent = context.vk->swapchain_extent();
    const VkExtent2D targetExtent = targets_->extent();
    if (flightCount == 0 || targets_->flightCount() != flightCount ||
        !isSrgbSwapchainFormat(context.vk->swapchain_format()) ||
        swapchainExtent.width == 0 || swapchainExtent.height == 0 ||
        targetExtent.width != swapchainExtent.width ||
        targetExtent.height != swapchainExtent.height) {
        throw std::invalid_argument(
            "world scene targets do not match the Pictor frame host");
    }

    impl_->context = &context;
    impl_->device = context.vk->device();
    try {
        const std::filesystem::path shaderDirectory(context.shader_dir);
        impl_->vertexShader =
            readSpirv(shaderDirectory / "konbini_composite.vert.spv");
        impl_->fragmentShader =
            readSpirv(shaderDirectory / "konbini_composite.frag.spv");

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.mipLodBias = 0.0F;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.minLod = 0.0F;
        samplerInfo.maxLod = 0.0F;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        VkResult result = vkCreateSampler(
            impl_->device, &samplerInfo, nullptr, &impl_->sampler);
        if (result != VK_SUCCESS) {
            failVulkan("vkCreateSampler", result);
        }

        const VkDescriptorSetLayoutBinding binding{
            .binding = 0,
            .descriptorType =
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        };
        const VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo{
            .sType =
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings = &binding,
        };
        result = vkCreateDescriptorSetLayout(
            impl_->device, &descriptorLayoutInfo, nullptr,
            &impl_->descriptorLayout);
        if (result != VK_SUCCESS) {
            failVulkan("vkCreateDescriptorSetLayout", result);
        }

        const VkDescriptorPoolSize poolSize{
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = flightCount,
        };
        const VkDescriptorPoolCreateInfo poolInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = flightCount,
            .poolSizeCount = 1,
            .pPoolSizes = &poolSize,
        };
        result = vkCreateDescriptorPool(
            impl_->device, &poolInfo, nullptr, &impl_->descriptorPool);
        if (result != VK_SUCCESS) {
            failVulkan("vkCreateDescriptorPool", result);
        }

        const std::vector<VkDescriptorSetLayout> layouts(
            flightCount, impl_->descriptorLayout);
        impl_->descriptorSets.resize(
            flightCount, VK_NULL_HANDLE);
        const VkDescriptorSetAllocateInfo allocateInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = impl_->descriptorPool,
            .descriptorSetCount = flightCount,
            .pSetLayouts = layouts.data(),
        };
        result = vkAllocateDescriptorSets(
            impl_->device, &allocateInfo,
            impl_->descriptorSets.data());
        if (result != VK_SUCCESS) {
            failVulkan("vkAllocateDescriptorSets", result);
        }

        std::vector<VkDescriptorImageInfo> imageInfos(flightCount);
        std::vector<VkWriteDescriptorSet> writes(flightCount);
        for (std::uint32_t flight = 0; flight < flightCount; ++flight) {
            const VkImageView colorView = targets_->colorView(flight);
            if (colorView == VK_NULL_HANDLE) {
                throw std::runtime_error(
                    "world scene target has no color view for a flight");
            }
            imageInfos[flight] = {
                .sampler = impl_->sampler,
                .imageView = colorView,
                .imageLayout =
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            };
            writes[flight] = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = impl_->descriptorSets[flight],
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType =
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &imageInfos[flight],
            };
        }
        vkUpdateDescriptorSets(
            impl_->device, flightCount, writes.data(), 0, nullptr);

        const VkPipelineLayoutCreateInfo pipelineLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &impl_->descriptorLayout,
        };
        result = vkCreatePipelineLayout(
            impl_->device, &pipelineLayoutInfo, nullptr,
            &impl_->pipelineLayout);
        if (result != VK_SUCCESS) {
            failVulkan("vkCreatePipelineLayout", result);
        }
    } catch (...) {
        shutdown();
        throw;
    }
    impl_->targetsGeneration = targets_->generation();
    impl_->initialized = true;
}

// @implements spec/interface/pictor-rendering.md Offscreen world composition
void WorldCompositeLayer::set_render_pass(
    const VkRenderPass renderPass) {
    if (!impl_->initialized || impl_->context == nullptr ||
        impl_->context->vk == nullptr ||
        !impl_->context->vk->is_initialized() ||
        impl_->context->vk->device() != impl_->device ||
        !isSrgbSwapchainFormat(
            impl_->context->vk->swapchain_format()) ||
        targets_ == nullptr || !targets_->isInitialized() ||
        targets_->generation() != impl_->targetsGeneration ||
        targets_->flightCount() !=
            impl_->context->vk->frames_in_flight() ||
        renderPass == VK_NULL_HANDLE ||
        renderPass != impl_->context->vk->default_render_pass()) {
        throw std::invalid_argument(
            "world composite layer requires the Pictor default render pass");
    }
    if (renderPass == impl_->renderPass &&
        impl_->pipeline != VK_NULL_HANDLE) {
        return;
    }

    VkShaderModule vertexModule = VK_NULL_HANDLE;
    VkShaderModule fragmentModule = VK_NULL_HANDLE;
    VkPipeline replacement = VK_NULL_HANDLE;
    try {
        vertexModule =
            createShaderModule(impl_->device, impl_->vertexShader);
        fragmentModule =
            createShaderModule(impl_->device, impl_->fragmentShader);

        ::pictor::GraphicsPipelineDesc pipelineDescription;
        pipelineDescription.vert = vertexModule;
        pipelineDescription.frag = fragmentModule;
        pipelineDescription.render_pass = renderPass;
        pipelineDescription.subpass = 0;
        pipelineDescription.layout = impl_->pipelineLayout;
        pipelineDescription.cull_back = false;
        pipelineDescription.depth_test = false;
        if (!::pictor::build_graphics_pipeline(
                impl_->device, pipelineDescription, replacement)) {
            throw std::runtime_error(
                "failed to create the world composite graphics pipeline");
        }
    } catch (...) {
        if (fragmentModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(
                impl_->device, fragmentModule, nullptr);
        }
        if (vertexModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(
                impl_->device, vertexModule, nullptr);
        }
        if (replacement != VK_NULL_HANDLE) {
            vkDestroyPipeline(impl_->device, replacement, nullptr);
        }
        throw;
    }
    vkDestroyShaderModule(impl_->device, fragmentModule, nullptr);
    vkDestroyShaderModule(impl_->device, vertexModule, nullptr);

    if (impl_->pipeline != VK_NULL_HANDLE) {
        impl_->context->vk->device_wait_idle();
        vkDestroyPipeline(impl_->device, impl_->pipeline, nullptr);
    }
    impl_->pipeline = replacement;
    impl_->renderPass = renderPass;
}

// @implements spec/interface/pictor-rendering.md Offscreen world composition
void WorldCompositeLayer::record(
    const VkCommandBuffer commandBuffer, const VkExtent2D extent) {
    if (!impl_->initialized || impl_->context == nullptr ||
        impl_->context->vk == nullptr ||
        !impl_->context->vk->is_initialized() ||
        impl_->context->vk->device() != impl_->device ||
        targets_ == nullptr || !targets_->isInitialized() ||
        targets_->flightCount() !=
            impl_->context->vk->frames_in_flight() ||
        commandBuffer == VK_NULL_HANDLE ||
        impl_->pipeline == VK_NULL_HANDLE) {
        throw std::logic_error(
            "world composite layer record called before initialization");
    }

    const VkExtent2D currentExtent =
        impl_->context->vk->swapchain_extent();
    // After a WorldSceneTargets::resize() every extent guard below re-converges
    // on the new extent, so none of them can see that the descriptors still
    // reference the destroyed views. The generation check is what catches it.
    if (extent.width == 0 || extent.height == 0 ||
        extent.width != currentExtent.width ||
        extent.height != currentExtent.height ||
        extent.width != targets_->extent().width ||
        extent.height != targets_->extent().height ||
        targets_->generation() != impl_->targetsGeneration ||
        impl_->renderPass !=
            impl_->context->vk->default_render_pass()) {
        throw std::runtime_error(
            "world composite layer has stale render targets");
    }

    const std::uint32_t flight =
        impl_->context->vk->current_frame();
    if (flight >= impl_->descriptorSets.size() ||
        impl_->descriptorSets[flight] == VK_NULL_HANDLE) {
        throw std::runtime_error(
            "Pictor current flight exceeds composite descriptors");
    }

    vkCmdBindPipeline(
        commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        impl_->pipeline);
    vkCmdBindDescriptorSets(
        commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        impl_->pipelineLayout, 0, 1,
        &impl_->descriptorSets[flight], 0, nullptr);

    const VkViewport viewport{
        .x = 0.0F,
        .y = 0.0F,
        .width = static_cast<float>(extent.width),
        .height = static_cast<float>(extent.height),
        .minDepth = 0.0F,
        .maxDepth = 1.0F,
    };
    const VkRect2D scissor{
        .offset = {0, 0},
        .extent = extent,
    };
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
}

// @implements spec/interface/pictor-rendering.md Offscreen world composition
void WorldCompositeLayer::shutdown() {
    if (impl_ == nullptr || impl_->context == nullptr) {
        return;
    }

    if (impl_->context->vk == nullptr ||
        !impl_->context->vk->is_initialized() ||
        impl_->context->vk->device() != impl_->device) {
        // Raw Vulkan handles cannot be abandoned safely. The required
        // lifetime is composite layer -> scene targets -> VulkanContext.
        std::terminate();
    }

    impl_->context->vk->device_wait_idle();
    if (impl_->pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(
            impl_->device, impl_->pipeline, nullptr);
    }
    if (impl_->pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(
            impl_->device, impl_->pipelineLayout, nullptr);
    }
    if (impl_->descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(
            impl_->device, impl_->descriptorPool, nullptr);
    }
    if (impl_->descriptorLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(
            impl_->device, impl_->descriptorLayout, nullptr);
    }
    if (impl_->sampler != VK_NULL_HANDLE) {
        vkDestroySampler(
            impl_->device, impl_->sampler, nullptr);
    }

    impl_->pipeline = VK_NULL_HANDLE;
    impl_->pipelineLayout = VK_NULL_HANDLE;
    impl_->descriptorPool = VK_NULL_HANDLE;
    impl_->descriptorLayout = VK_NULL_HANDLE;
    impl_->sampler = VK_NULL_HANDLE;
    impl_->descriptorSets.clear();
    impl_->vertexShader.clear();
    impl_->fragmentShader.clear();
    impl_->renderPass = VK_NULL_HANDLE;
    impl_->targetsGeneration = 0;
    impl_->device = VK_NULL_HANDLE;
    impl_->context = nullptr;
    impl_->initialized = false;
}

}  // namespace konbini::render
