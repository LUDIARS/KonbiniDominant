#version 450

// World pass vertex shader for objects drawn through Pictor's
// CompiledBatchRecorder (pictor-rendering.md#Required bridge).
//
// Vertex layout must match konbini::render::WorldVertex
// (position: vec3, normal: vec3, color: vec4; 40 bytes, tightly packed).
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;

// viewProjection is column-major, right-handed view space, Vulkan clip space
// (depth 0..1, Y down). The CPU source is konbini::render::IsometricCamera.
layout(push_constant) uniform WorldInstancedPush {
    mat4 viewProjection;
} push;

// The recorder draws with firstInstance = batch.startIndex, the object's
// position in Pictor's sorted DYNAMIC pool order, so gl_InstanceIndex
// addresses konbini::adapters::pictor::WorldInstanceRecord directly.
struct WorldInstance {
    mat4 model;
    vec4 tint;
};

layout(std430, set = 0, binding = 0) readonly buffer WorldInstances {
    WorldInstance instances[];
};

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec4 outColor;

void main() {
    WorldInstance instance = instances[gl_InstanceIndex];
    // Models carry rotation + translation only (no non-uniform scale), so
    // the upper 3x3 transforms normals. A zero normal stays zero and the
    // fragment shader treats it as unshaded.
    outNormal = mat3(instance.model) * inNormal;
    outColor = inColor * instance.tint;
    gl_Position = push.viewProjection * (instance.model * vec4(inPosition, 1.0));
}
