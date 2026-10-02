#pragma once

#include <array>

#ifndef NOGDI
#define NOGDI
#endif
#include "pictor/core/types.h"

// @implements spec/interface/pictor-rendering.md Game-owned render domain

namespace konbini::adapters::pictor {

// Pictor culling frustum from the camera's view-projection.
//
// `viewProjection` follows the render-domain convention: column-major
// (index = col * 4 + row), Vulkan clip space with depth 0..1. The planes
// (left, right, bottom, top, near, far) point inward and are normalized, so
// `Frustum::test_aabb` keeps an AABB when it is at least partly inside.
// Non-finite or degenerate matrices are `std::invalid_argument`; culling with
// them would hide the whole city or nothing at all without saying so.
[[nodiscard]] ::pictor::Frustum frustumFromViewProjection(
    const std::array<float, 16>& viewProjection);

}  // namespace konbini::adapters::pictor
