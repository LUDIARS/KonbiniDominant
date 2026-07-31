#pragma once

#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "konbini/render/visia_geometry.h"

// @implements spec/interface/visia-presentation.md CPU primitive geometry

namespace konbini::render::detail {

[[nodiscard]] inline sim::Vec3 add(const sim::Vec3 left,
                                   const sim::Vec3 right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] inline sim::Vec3 scale(const sim::Vec3 value,
                                     const double scalar) noexcept {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

[[nodiscard]] inline float checkedFloat(const double value) {
    constexpr double kMaximum =
        static_cast<double>(std::numeric_limits<float>::max());
    if (!std::isfinite(value) || value < -kMaximum || value > kMaximum) {
        throw std::invalid_argument(
            "Visia geometry value cannot be represented as float");
    }
    return static_cast<float>(value);
}

[[nodiscard]] inline bool isValidColor(
    const std::array<float, 4>& color) noexcept {
    for (const float component : color) {
        if (!std::isfinite(component) || component < 0.0F ||
            component > 1.0F) {
            return false;
        }
    }
    return color[3] > 0.0F;
}

inline void appendVertex(VisiaGeometry& geometry,
                         const sim::Vec3 position,
                         const sim::Vec3 normal,
                         const std::array<float, 4>& color) {
    geometry.vertices.push_back({
        .position = {
            checkedFloat(position.x),
            checkedFloat(position.y),
            checkedFloat(position.z),
        },
        .normal = {
            checkedFloat(normal.x),
            checkedFloat(normal.y),
            checkedFloat(normal.z),
        },
        .color = color,
    });
}

}  // namespace konbini::render::detail
