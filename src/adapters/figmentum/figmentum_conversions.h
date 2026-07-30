#pragma once

#include "figmentum/gen/building.h"

#include "konbini/sim/math_types.h"

// Figmentum の float 幾何値を sim 側の meter 座標へ移す唯一の変換点。
// projection と meshing で別実装を持つと、同じ recipe から別 bounds が出て
// cache key と manifest が食い違う。overload ではなく別名にするのは、
// 各 TU の anonymous namespace 側 `convert` に隠されないようにするため。
// @implements spec/interface/figmentum-city-generation.md Output: `CityManifest`
// @implements spec/interface/figmentum-city-generation.md Geometry generation

namespace konbini::adapters::figmentum {

// @implements spec/interface/figmentum-city-generation.md Output: `CityManifest`
[[nodiscard]] inline sim::Vec3 toVec3(const fg::Vec3 value) noexcept {
    return {static_cast<double>(value.x),
            static_cast<double>(value.y),
            static_cast<double>(value.z)};
}

// @implements spec/interface/figmentum-city-generation.md Output: `CityManifest`
[[nodiscard]] inline sim::Bounds3 toBounds3(const fg::Aabb& bounds) noexcept {
    return {toVec3(bounds.min), toVec3(bounds.max)};
}

}  // namespace konbini::adapters::figmentum
