#pragma once

#include "konbini/render/world_vertex.h"

#ifndef NOGDI
#define NOGDI
#endif
#include <vulkan/vulkan.h>

// @implements spec/interface/pictor-rendering.md World pass recording

namespace konbini::adapters::pictor {

// world pass の pipeline 差分。base は depth write あり / blend なし、
// overlay (半透明) は depth write なし / alpha blend あり。
struct WorldPipelineState {
    bool depthWrite = false;
    bool blend = false;
};

// `render::WorldVertex` を 1 binding で読む world pass 用 graphics pipeline。
// cull none、depth test LESS_OR_EQUAL、viewport / scissor は dynamic。
// push constant 版 (`WorldPipelines`) と instance data 版
// (`WorldInstancedPipelines`) が同じ固定 state を共有するため 1 箇所に置く。
// 失敗は `std::runtime_error`。生成した pipeline の破棄は呼び出し側の責務。
[[nodiscard]] VkPipeline createWorldPipeline(
    VkDevice device, VkRenderPass renderPass, VkPipelineLayout layout,
    VkShaderModule vertexModule, VkShaderModule fragmentModule,
    const WorldPipelineState& state);

}  // namespace konbini::adapters::pictor
