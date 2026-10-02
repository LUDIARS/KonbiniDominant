#pragma once

#include <cstdint>

// @implements spec/interface/pictor-rendering.md Required bridge

namespace konbini::adapters::pictor {

// Pictor `shaderKey`s for world objects drawn through the bridge.
//
// `BatchBuilder` sorts on bits 48..63 of the shader key, so the variants
// live there; the low 32 bits stay free for Pictor's own use. Translucent
// facilities (palette alpha < 1, BASE-FP-DESTROYED-01) get their own key so
// their batches never share a pipeline with the depth-writing base pass.
inline constexpr std::uint64_t kOpaqueWorldShaderKey = 1ULL << 48U;
inline constexpr std::uint64_t kTranslucentWorldShaderKey = 2ULL << 48U;

}  // namespace konbini::adapters::pictor
