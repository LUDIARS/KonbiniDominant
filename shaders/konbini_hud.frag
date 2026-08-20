#version 450

layout(location = 0) in vec4 inColor;

layout(location = 0) out vec4 outColor;

// The HUD is authored in the swapchain's own colour space and composited with
// straight alpha over the world composite result. No lighting is applied.
void main() {
    outColor = inColor;
}
