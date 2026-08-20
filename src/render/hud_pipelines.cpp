#include "hud_pipelines.h"

#include <cstddef>
#include <stdexcept>
#include <string>

#include "konbini/adapters/pictor/spirv_module.h"

// @implements spec/feature/ui-ux.md Common HUD

namespace konbini::render {

namespace {

[[noreturn]] void failVulkan(const char* operation, const VkResult result) {
    throw std::runtime_error(
        std::string(operation) + " failed with VkResult " +
        std::to_string(static_cast<int>(result)));
}

// shader の `layout(location = ...)` と 1 対 1。HUD は world pass と同じ
// `WorldVertex` を流すので、attribute も同じ 3 本にする。
[[nodiscard]] std::array<VkVertexInputAttributeDescription, 3>
hudVertexAttributes() noexcept {
    return {
        VkVertexInputAttributeDescription{
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset =
                static_cast<std::uint32_t>(offsetof(WorldVertex, position)),
        },
        VkVertexInputAttributeDescription{
            .location = 1,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset =
                static_cast<std::uint32_t>(offsetof(WorldVertex, normal)),
        },
        VkVertexInputAttributeDescription{
            .location = 2,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset =
                static_cast<std::uint32_t>(offsetof(WorldVertex, color)),
        },
    };
}

[[nodiscard]] VkPipeline createHudPipeline(
    const VkDevice device, const VkRenderPass renderPass,
    const VkPipelineLayout layout, const VkShaderModule vertexModule,
    const VkShaderModule fragmentModule) {
    const VkPipelineShaderStageCreateInfo stages[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertexModule,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragmentModule,
            .pName = "main",
        },
    };

    const VkVertexInputBindingDescription binding{
        .binding = 0,
        .stride = static_cast<std::uint32_t>(sizeof(WorldVertex)),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    const auto attributes = hudVertexAttributes();
    const VkPipelineVertexInputStateCreateInfo vertexInput{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding,
        .vertexAttributeDescriptionCount =
            static_cast<std::uint32_t>(attributes.size()),
        .pVertexAttributeDescriptions = attributes.data(),
    };

    const VkPipelineInputAssemblyStateCreateInfo inputAssembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };
    const VkPipelineViewportStateCreateInfo viewport{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };
    // HUD quad の winding は builder 側で固定しているが、depth も cull も
    // 使わない方が pass の前提が減る。
    const VkPipelineRasterizationStateCreateInfo rasterization{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0F,
    };
    const VkPipelineMultisampleStateCreateInfo multisample{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };
    // 既定 swapchain pass は depth attachment を持たないので、depth state は
    // 完全に無効にする。
    const VkPipelineDepthStencilStateCreateInfo depthStencil{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_FALSE,
        .depthWriteEnable = VK_FALSE,
        .depthCompareOp = VK_COMPARE_OP_ALWAYS,
    };

    const VkPipelineColorBlendAttachmentState blendAttachment{
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    const VkPipelineColorBlendStateCreateInfo colorBlend{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blendAttachment,
    };

    const VkDynamicState dynamicStates[2] = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    const VkPipelineDynamicStateCreateInfo dynamic{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamicStates,
    };

    const VkGraphicsPipelineCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = stages,
        .pVertexInputState = &vertexInput,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewport,
        .pRasterizationState = &rasterization,
        .pMultisampleState = &multisample,
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlend,
        .pDynamicState = &dynamic,
        .layout = layout,
        .renderPass = renderPass,
        .subpass = 0,
    };
    VkPipeline pipeline = VK_NULL_HANDLE;
    const VkResult result = vkCreateGraphicsPipelines(
        device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);
    if (result != VK_SUCCESS) {
        failVulkan("vkCreateGraphicsPipelines", result);
    }
    return pipeline;
}

}  // namespace

HudPipelines::~HudPipelines() {
    shutdown();
}

// @implements spec/interface/pictor-rendering.md Failure
void HudPipelines::initialize(
    const VkDevice device, const std::filesystem::path& shaderDirectory) {
    if (isInitialized()) {
        throw std::logic_error("hud pipelines are already initialized");
    }
    if (device == VK_NULL_HANDLE || shaderDirectory.empty()) {
        throw std::invalid_argument(
            "hud pipelines require a device and a shader directory");
    }

    device_ = device;
    try {
        vertexShader_ = adapters::pictor::readSpirv(
            shaderDirectory / "konbini_hud.vert.spv");
        fragmentShader_ = adapters::pictor::readSpirv(
            shaderDirectory / "konbini_hud.frag.spv");

        const VkPushConstantRange pushConstants{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = static_cast<std::uint32_t>(sizeof(HudPushConstants)),
        };
        const VkPipelineLayoutCreateInfo layoutInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
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

void HudPipelines::setRenderPass(const VkRenderPass renderPass) {
    if (!isInitialized()) {
        throw std::logic_error(
            "hud pipelines require initialization before a render pass");
    }
    if (renderPass == VK_NULL_HANDLE) {
        throw std::invalid_argument(
            "hud pipelines require a valid render pass");
    }
    if (renderPass == renderPass_ && hasPipeline()) {
        return;
    }

    VkShaderModule vertexModule = VK_NULL_HANDLE;
    VkShaderModule fragmentModule = VK_NULL_HANDLE;
    VkPipeline replacement = VK_NULL_HANDLE;
    try {
        vertexModule =
            adapters::pictor::createShaderModule(device_, vertexShader_);
        fragmentModule =
            adapters::pictor::createShaderModule(device_, fragmentShader_);
        replacement = createHudPipeline(
            device_, renderPass, layout_, vertexModule, fragmentModule);
    } catch (...) {
        if (replacement != VK_NULL_HANDLE) {
            vkDestroyPipeline(device_, replacement, nullptr);
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

    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, pipeline_, nullptr);
    }
    pipeline_ = replacement;
    renderPass_ = renderPass;
}

void HudPipelines::shutdown() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
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

bool HudPipelines::isInitialized() const noexcept {
    return device_ != VK_NULL_HANDLE && layout_ != VK_NULL_HANDLE;
}

bool HudPipelines::hasPipeline() const noexcept {
    return pipeline_ != VK_NULL_HANDLE;
}

VkRenderPass HudPipelines::renderPass() const noexcept {
    return renderPass_;
}

VkPipelineLayout HudPipelines::layout() const {
    if (!isInitialized()) {
        throw std::logic_error(
            "hud pipeline layout requested before initialization");
    }
    return layout_;
}

VkPipeline HudPipelines::pipeline() const {
    if (!hasPipeline()) {
        throw std::logic_error(
            "hud pipeline requested before a render pass was set");
    }
    return pipeline_;
}

}  // namespace konbini::render
