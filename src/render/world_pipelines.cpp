#include "world_pipelines.h"

#include <cstddef>
#include <stdexcept>
#include <string>

#include "konbini/adapters/pictor/spirv_module.h"
#include "konbini/adapters/pictor/world_pipeline_factory.h"

// @implements spec/interface/pictor-rendering.md Offscreen world composition

namespace konbini::render {

namespace {

[[noreturn]] void failVulkan(const char* operation, const VkResult result) {
    throw std::runtime_error(
        std::string(operation) + " failed with VkResult " +
        std::to_string(static_cast<int>(result)));
}

}  // namespace

WorldPipelines::~WorldPipelines() {
    shutdown();
}

// @implements spec/interface/pictor-rendering.md Failure
void WorldPipelines::initialize(
    const VkDevice device, const std::filesystem::path& shaderDirectory) {
    if (isInitialized()) {
        throw std::logic_error("world pipelines are already initialized");
    }
    if (device == VK_NULL_HANDLE || shaderDirectory.empty()) {
        throw std::invalid_argument(
            "world pipelines require a device and a shader directory");
    }

    device_ = device;
    try {
        vertexShader_ = adapters::pictor::readSpirv(
            shaderDirectory / "konbini_world.vert.spv");
        fragmentShader_ = adapters::pictor::readSpirv(
            shaderDirectory / "konbini_world.frag.spv");

        // tint も vertex shader で掛けるので、push constant は vertex stage
        // だけが読む。
        const VkPushConstantRange pushConstants{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = static_cast<std::uint32_t>(sizeof(WorldPushConstants)),
        };
        const VkPipelineLayoutCreateInfo layoutInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pushConstants,
        };
        const VkResult result = vkCreatePipelineLayout(
            device_, &layoutInfo, nullptr, &layout_);
        if (result != VK_SUCCESS) {
            failVulkan("vkCreatePipelineLayout", result);
        }
    } catch (...) {
        shutdown();
        throw;
    }
}

// @implements spec/interface/pictor-rendering.md Offscreen world composition
void WorldPipelines::setRenderPass(const VkRenderPass renderPass) {
    if (!isInitialized()) {
        throw std::logic_error(
            "world pipelines require initialization before a render pass");
    }
    if (renderPass == VK_NULL_HANDLE) {
        throw std::invalid_argument(
            "world pipelines require a valid render pass");
    }
    if (renderPass == renderPass_ && hasPipelines()) {
        return;
    }

    VkShaderModule vertexModule = VK_NULL_HANDLE;
    VkShaderModule fragmentModule = VK_NULL_HANDLE;
    VkPipeline base = VK_NULL_HANDLE;
    VkPipeline overlay = VK_NULL_HANDLE;
    try {
        vertexModule =
            adapters::pictor::createShaderModule(device_, vertexShader_);
        fragmentModule =
            adapters::pictor::createShaderModule(device_, fragmentShader_);
        base = adapters::pictor::createWorldPipeline(
            device_, renderPass, layout_, vertexModule, fragmentModule,
            {.depthWrite = true, .blend = false});
        overlay = adapters::pictor::createWorldPipeline(
            device_, renderPass, layout_, vertexModule, fragmentModule,
            {.depthWrite = false, .blend = true});
    } catch (...) {
        if (overlay != VK_NULL_HANDLE) {
            vkDestroyPipeline(device_, overlay, nullptr);
        }
        if (base != VK_NULL_HANDLE) {
            vkDestroyPipeline(device_, base, nullptr);
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

    // 2 本とも揃ってから旧世代を捨てる。
    if (overlay_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, overlay_, nullptr);
    }
    if (base_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, base_, nullptr);
    }
    base_ = base;
    overlay_ = overlay;
    renderPass_ = renderPass;
}

void WorldPipelines::shutdown() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    if (overlay_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, overlay_, nullptr);
        overlay_ = VK_NULL_HANDLE;
    }
    if (base_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, base_, nullptr);
        base_ = VK_NULL_HANDLE;
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

bool WorldPipelines::isInitialized() const noexcept {
    return device_ != VK_NULL_HANDLE && layout_ != VK_NULL_HANDLE;
}

bool WorldPipelines::hasPipelines() const noexcept {
    return base_ != VK_NULL_HANDLE && overlay_ != VK_NULL_HANDLE;
}

VkRenderPass WorldPipelines::renderPass() const noexcept {
    return renderPass_;
}

VkPipelineLayout WorldPipelines::layout() const {
    if (!isInitialized()) {
        throw std::logic_error(
            "world pipeline layout requested before initialization");
    }
    return layout_;
}

VkPipeline WorldPipelines::basePipeline() const {
    if (!hasPipelines()) {
        throw std::logic_error(
            "world base pipeline requested before a render pass was set");
    }
    return base_;
}

VkPipeline WorldPipelines::overlayPipeline() const {
    if (!hasPipelines()) {
        throw std::logic_error(
            "world overlay pipeline requested before a render pass was set");
    }
    return overlay_;
}

}  // namespace konbini::render
