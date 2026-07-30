#pragma once

#include <cmath>

// @implements spec/data/world-state.md 座標
// @implements spec/interface/figmentum-city-generation.md Error contract

namespace konbini::sim {

// Figmentum recipe / manifest の geometry 値を検証するための meter 座標。
// gameplay の正本位置は `WorldPosition` (dimension / xMeters / zMeters /
// verticalSlot) 側であり、浮動小数の派生値を save 正本にしない。
struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Bounds3 {
    Vec3 min{};
    Vec3 max{};
};

// @implements spec/data/world-state.md 座標
[[nodiscard]] inline bool isFinite(const Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

// 各軸で厳密に min < max を要求する。extent 0 の退化 bounds は
// selection / culling として使えないため invalid 扱いにする。
// @implements spec/interface/figmentum-city-generation.md Error contract
[[nodiscard]] inline bool isFiniteAndOrdered(const Bounds3& bounds) noexcept {
    return isFinite(bounds.min) && isFinite(bounds.max) &&
           bounds.min.x < bounds.max.x && bounds.min.y < bounds.max.y &&
           bounds.min.z < bounds.max.z;
}

}  // namespace konbini::sim
