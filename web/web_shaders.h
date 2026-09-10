#pragma once
namespace konbini::web {
inline constexpr const char* kVertexShader = R"(#version 300 es
precision highp float;
layout(location=0) in vec3 a_position;
layout(location=1) in vec3 a_normal;
layout(location=2) in vec4 a_color;
uniform mat4 u_view_projection;
uniform vec2 u_extent;
uniform bool u_hud;
out vec3 v_normal;
out vec4 v_color;
void main() {
    v_normal = a_normal;
    v_color = a_color;
    if (u_hud) {
        vec2 p = a_position.xy / u_extent;
        gl_Position = vec4(p.x * 2.0 - 1.0, 1.0 - p.y * 2.0, 0.0, 1.0);
    } else {
        vec4 p = u_view_projection * vec4(a_position, 1.0);
        gl_Position = vec4(p.x, -p.y, 2.0 * p.z - p.w, p.w);
    }
}
)";
inline constexpr const char* kFragmentShader = R"(#version 300 es
precision highp float;
uniform bool u_hud;
in vec3 v_normal;
in vec4 v_color;
out vec4 fragment;
vec3 encodeSrgb(vec3 linear) {
    linear = max(linear, vec3(0.0));
    return mix(12.92 * linear, 1.055 * pow(linear, vec3(1.0/2.4)) - 0.055,
               step(vec3(0.0031308), linear));
}
void main() {
    if (u_hud) { fragment = v_color; return; }
    float n = dot(v_normal, v_normal);
    float lambert = n > 0.0 ? max(dot(v_normal * inversesqrt(n),
                        vec3(-0.4082483, 0.8164966, -0.4082483)), 0.0) : 1.0;
    fragment = vec4(encodeSrgb(v_color.rgb * (0.35 + 0.65 * lambert)), v_color.a);
}
)";
}
