#pragma once

#include <cstdint>
#include <vector>

#include "konbini/render/presentation_draw.h"
#include "konbini/render/world_mesh.h"

// @implements spec/interface/pictor-rendering.md Presentation objects
// @implements spec/interface/visia-presentation.md Pictor integration boundary

namespace konbini::render {

// The landing ring's radius / alpha animation is baked into this many age
// frames (BASE-NPC-RING-FRAMES-01): Pictor objects carry a rigid transform
// and a tint, and the ring's inner and outer radii grow at different rates.
inline constexpr std::uint32_t kLandingRingFrameCount = 16;

// Frame whose baked age is nearest to `normalizedAge` in [0, 1].
[[nodiscard]] std::uint32_t landingRingFrame(double normalizedAge);

struct PresentationMesh {
    PresentationMeshKey key{};
    WorldMesh mesh;
};

// Every shared presentation mesh, uploaded once at startup:
// - ResidentBody: the ResidentPrimitive Visia at the origin, yaw 0
// - LandingRingFrame 0..kLandingRingFrameCount-1 at the origin
// - BubbleBackground: unit quad x in [-0.5, 0.5], y in [0, 1], white
// - BubbleTail: triangle (-1, 0), (1, 0), (0, -1), white
// - SpeechGlyph: one mesh per atlas glyph that has triangles, white
// Bubble and glyph meshes have zero normals (unshaded); their colour comes
// from the per-object tint.
[[nodiscard]] std::vector<PresentationMesh> buildPresentationMeshes();

}  // namespace konbini::render
