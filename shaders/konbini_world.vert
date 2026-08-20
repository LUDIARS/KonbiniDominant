#version 450

// Vertex layout must match konbini::render::WorldVertex
// (position: vec3, normal: vec3, color: vec4; 40 bytes, tightly packed).
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;

// viewProjection is column-major, right-handed view space, Vulkan clip space
// (depth 0..1, Y down). The CPU source is konbini::render::IsometricCamera.
layout(push_constant) uniform WorldPush {
    mat4 viewProjection;
    vec4 tint;
} push;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec4 outColor;

void main() {
    outNormal = inNormal;
    outColor = inColor * push.tint;
    gl_Position = push.viewProjection * vec4(inPosition, 1.0);
}
