#include "konbini/adapters/pictor/world_frustum.h"

#include <cmath>
#include <cstddef>
#include <stdexcept>

// @implements spec/interface/pictor-rendering.md Game-owned render domain

namespace konbini::adapters::pictor {

namespace {

struct Row {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    float w = 0.0F;
};

[[nodiscard]] Row row(const std::array<float, 16>& m, const std::size_t r) {
    return {m[0 * 4 + r], m[1 * 4 + r], m[2 * 4 + r], m[3 * 4 + r]};
}

[[nodiscard]] ::pictor::Plane plane(const Row& p) {
    const float length = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
    if (!std::isfinite(length) || length <= 0.0F || !std::isfinite(p.w)) {
        throw std::invalid_argument(
            "camera view-projection yields a degenerate frustum plane");
    }
    ::pictor::Plane result;
    result.normal = {p.x / length, p.y / length, p.z / length};
    result.distance = p.w / length;
    return result;
}

[[nodiscard]] Row add(const Row& a, const Row& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
}

[[nodiscard]] Row subtract(const Row& a, const Row& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w};
}

}  // namespace

// @implements spec/interface/pictor-rendering.md Game-owned render domain
::pictor::Frustum frustumFromViewProjection(
    const std::array<float, 16>& viewProjection) {
    for (const float value : viewProjection) {
        if (!std::isfinite(value)) {
            throw std::invalid_argument(
                "camera view-projection has a non-finite element");
        }
    }
    const Row x = row(viewProjection, 0);
    const Row y = row(viewProjection, 1);
    const Row z = row(viewProjection, 2);
    const Row w = row(viewProjection, 3);

    ::pictor::Frustum frustum;
    frustum.planes[0] = plane(add(w, x));       // left:   -w <= x
    frustum.planes[1] = plane(subtract(w, x));  // right:   x <= w
    frustum.planes[2] = plane(add(w, y));       // bottom: -w <= y
    frustum.planes[3] = plane(subtract(w, y));  // top:     y <= w
    frustum.planes[4] = plane(z);               // near:    0 <= z (Vulkan)
    frustum.planes[5] = plane(subtract(w, z));  // far:     z <= w
    return frustum;
}

}  // namespace konbini::adapters::pictor
