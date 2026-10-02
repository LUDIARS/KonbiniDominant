#pragma once

#include <array>
#include <cmath>

#include "konbini/sim/math_types.h"

// @implements spec/interface/pictor-rendering.md Presentation objects

namespace konbini::render {

using ColumnMajorMatrix = std::array<float, 16>;

// Model whose local x / y / z axes map to `xAxis` / `yAxis` / `zAxis` (scale
// included) and whose origin maps to `origin`. Column-major.
[[nodiscard]] inline ColumnMajorMatrix basisModel(
    const sim::Vec3& xAxis, const sim::Vec3& yAxis, const sim::Vec3& zAxis,
    const sim::Vec3& origin) noexcept {
    const auto f = [](const double value) { return static_cast<float>(value); };
    return {
        f(xAxis.x), f(xAxis.y), f(xAxis.z), 0.0F,
        f(yAxis.x), f(yAxis.y), f(yAxis.z), 0.0F,
        f(zAxis.x), f(zAxis.y), f(zAxis.z), 0.0F,
        f(origin.x), f(origin.y), f(origin.z), 1.0F,
    };
}

// Yaw about +Y in the Visia convention (0 faces +Z, positive turns toward +X:
// x' = cos * x + sin * z, z' = -sin * x + cos * z), then translation.
[[nodiscard]] inline ColumnMajorMatrix yawTranslationModel(
    const sim::Vec3& position, const double yawDegrees) noexcept {
    constexpr double kDegreesToRadians = 0.017453292519943295769236907684886;
    const double radians = yawDegrees * kDegreesToRadians;
    const double c = std::cos(radians);
    const double s = std::sin(radians);
    return basisModel({c, 0.0, -s}, {0.0, 1.0, 0.0}, {s, 0.0, c}, position);
}

[[nodiscard]] inline sim::Vec3 transformPoint(
    const ColumnMajorMatrix& model, const sim::Vec3& point) noexcept {
    return {
        model[0] * point.x + model[4] * point.y + model[8] * point.z + model[12],
        model[1] * point.x + model[5] * point.y + model[9] * point.z + model[13],
        model[2] * point.x + model[6] * point.y + model[10] * point.z + model[14],
    };
}

}  // namespace konbini::render
