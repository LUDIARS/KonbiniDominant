#version 450

// Vertex layout must match konbini::render::WorldVertex
// (position: vec3, normal: vec3, color: vec4; 40 bytes, tightly packed).
// The HUD reuses that layout so the per-flight overlay buffer owner can be
// shared with the world pass; `inNormal` is unused here.
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;

// HUD geometry arrives in framebuffer pixels with the origin at the top-left
// and Y pointing down, matching Vulkan's framebuffer coordinates. Converting
// on the GPU keeps the CPU mesh independent of the swapchain extent.
// @implements spec/feature/ui-ux.md Common HUD
// @spec Common HUD
layout(push_constant) uniform HudPush {
    vec4 viewportPixels;
} push;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = inColor;
    vec2 normalized = inPosition.xy / push.viewportPixels.xy;
    gl_Position = vec4(normalized * 2.0 - 1.0, 0.0, 1.0);
}
