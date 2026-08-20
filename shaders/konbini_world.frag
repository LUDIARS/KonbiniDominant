#version 450

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 outColor;

// Fixed key light for the first playable. The scene colour target is linear
// R16G16B16A16_SFLOAT and the composite pass writes it straight to an sRGB
// swapchain, so no encoding happens here.
const vec3 kKeyLightDirection = vec3(-0.4082483, 0.8164966, -0.4082483);
const float kAmbient = 0.35;

void main() {
    // Marching-cubes can emit a degenerate normal, and overlay geometry is
    // authored flat. Both arrive as a zero vector, which must not become a
    // NaN through normalize(); treat them as unshaded instead.
    float lengthSquared = dot(inNormal, inNormal);
    float lambert = 1.0;
    if (lengthSquared > 0.0) {
        vec3 unitNormal = inNormal * inversesqrt(lengthSquared);
        lambert = max(dot(unitNormal, kKeyLightDirection), 0.0);
    }
    float intensity = kAmbient + (1.0 - kAmbient) * lambert;
    outColor = vec4(inColor.rgb * intensity, inColor.a);
}
