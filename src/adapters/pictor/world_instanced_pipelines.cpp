#include "konbini/adapters/pictor/world_instanced_pipelines.h"

#include <stdexcept>
#include <string>

#include "konbini/adapters/pictor/spirv_module.h"
#include "konbini/adapters/pictor/world_pipeline_factory.h"

// @implements spec/interface/pictor-rendering.md World pass recording

namespace konbini::adapters::pictor {

namespace {

[[noreturn]] void failVulkan(const char* operation, const VkResult result) {
    throw std::runtime_error(
        std::string(operation) + " failed with VkResult " +
        std::to_string(static_cast<int>(result)));
}

}  // namespace

WorldInstancedPipelines::~WorldInstancedPipelines() {
    shutdown();
}

// @implements spec/interface/pictor-rendering.md Failure
void WorldInstancedPipelines::initialize(
    const VkDevice device, const std::filesystem::path& shaderDirectory,
    const VkDescriptorSetLayout instanceSetLayout) {
    if (isInitialized()) {
        throw std::logic_error(
            "world instanced pipelines are already initialized");
    }
    if (device == VK_NULL_HANDLE || shaderDirectory.empty() ||
        instanceSetLayout == VK_NULL_HANDLE) {
        throw std::invalid_argument(
            "world instanced pipelines require a device, shaders and a set "
            "layout");
    }

    device_ = device;
    try {
        vertexShader_ =
            readSpirv(shaderDirectory / "konbini_world_instanced.vert.spv");
        fragmentShader_ = readSpirv(shaderDirectory / "konbini_world.frag.spv");

        const VkPushConstantRange pushConstants{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = static_cast<std::uint32_t>(
                sizeof(WorldInstancedPushConstants)),
        };
        const VkPipelineLayoutCreateInfo layoutInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &instanceSetLayout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pushConstants,
        };
        const VkResult result =
            vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &layout_);
        if (result != VK_SUCCESS) {
            failVulkan("vkCreatePipelineLayout", result);
        }
    } catch (...) {
        shutdown();
        throw;
    }
}

// @implements spec/interface/pictor-rendering.md World pass recording
void WorldInstancedPipelines::setRenderPass(const VkRenderPass renderPass) {
    if (!isInitialized()) {
        throw std::logic_error(
            "world instanced pipelines require initialization first");
    }
    if (renderPass == VK_NULL_HANDLE) {
        throw std::invalid_argument(
            "world instanced pipelines require a valid render pass");
    }
    if (renderPass == renderPass_ && hasPipelines()) {
        return;
    }

    VkShaderModule vertexModule = VK_NULL_HANDLE;
    VkShaderModule fragmentModule = VK_NULL_HANDLE;
    VkPipeline opaque = VK_NULL_HANDLE;
    VkPipeline translucent = VK_NULL_HANDLE;
    try {
        vertexModule = createShaderModule(device_, vertexShader_);
        fragmentModule = createShaderModule(device_, fragmentShader_);
        opaque = createWorldPipeline(
            device_, renderPass, layout_, vertexModule, fragmentModule,
            {.depthWrite = true, .blend = false});
        translucent = createWorldPipeline(
            device_, renderPass, layout_, vertexModule, fragmentModule,
            {.depthWrite = false, .blend = true});
    } catch (...) {
        if (translucent != VK_NULL_HANDLE) {
            vkDestroyPipeline(device_, translucent, nullptr);
        }
        if (opaque != VK_NULL_HANDLE) {
            vkDestroyPipeline(device_, opaque, nullptr);
        }
        if (fragmentModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, fragmentModule, nullptr);
        }
        if (vertexModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, vertexModule, nullptr);
        }
        throw;
    }
    vkDestroyShaderModule(device_, fragmentModule, nullptr);
    vkDestroyShaderModule(device_, vertexModule, nullptr);

    if (translucent_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, translucent_, nullptr);
    }
    if (opaque_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, opaque_, nullptr);
    }
    opaque_ = opaque;
    translucent_ = translucent;
    renderPass_ = renderPass;
}

void WorldInstancedPipelines::shutdown() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    if (translucent_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, translucent_, nullptr);
        translucent_ = VK_NULL_HANDLE;
    }
    if (opaque_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, opaque_, nullptr);
        opaque_ = VK_NULL_HANDLE;
    }
    if (layout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, layout_, nullptr);
        layout_ = VK_NULL_HANDLE;
    }
    renderPass_ = VK_NULL_HANDLE;
    vertexShader_.clear();
    fragmentShader_.clear();
    device_ = VK_NULL_HANDLE;
}

bool WorldInstancedPipelines::isInitialized() const noexcept {
    return device_ != VK_NULL_HANDLE && layout_ != VK_NULL_HANDLE;
}

bool WorldInstancedPipelines::hasPipelines() const noexcept {
    return opaque_ != VK_NULL_HANDLE && translucent_ != VK_NULL_HANDLE;
}

VkRenderPass WorldInstancedPipelines::renderPass() const noexcept {
    return renderPass_;
}

VkPipelineLayout WorldInstancedPipelines::layout() const {
    if (!isInitialized()) {
        throw std::logic_error(
            "world instanced layout requested before initialization");
    }
    return layout_;
}

VkPipeline WorldInstancedPipelines::opaquePipeline() const {
    if (!hasPipelines()) {
        throw std::logic_error(
            "world instanced opaque pipeline requested before a render pass");
    }
    return opaque_;
}

VkPipeline WorldInstancedPipelines::translucentPipeline() const {
    if (!hasPipelines()) {
        throw std::logic_error(
            "world instanced translucent pipeline requested before a render "
            "pass");
    }
    return translucent_;
}

}  // namespace konbini::adapters::pictor
