#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

#include "konbini/render/world_mesh.h"
#include "konbini/sim/math_types.h"

// @implements spec/interface/pictor-rendering.md Game-owned render domain

namespace konbini::render::detail {

[[nodiscard]] inline float checkedOverlayFloat(const double value) {
    constexpr double kMaximum =
        static_cast<double>(std::numeric_limits<float>::max());
    if (!std::isfinite(value) || value < -kMaximum || value > kMaximum) {
        throw std::invalid_argument(
            "overlay coordinate cannot be represented as float");
    }
    return static_cast<float>(value);
}

// 軸平行 box を 6 面 24 vertex / 36 index で追加する。面ごとに vertex を
// 分けるのは normal を面単位で持たせるためで、共有すると陰影が平均化される。
// winding は外向き normal に対して CCW。resident の box は yaw 回転を伴う別
// 責務なので統合しない。
inline void appendAxisAlignedBox(
    WorldMesh& mesh, const sim::Vec3 centerMeters,
    const sim::Vec3 halfExtentsMeters, const WorldVertex::ColorRgba& color) {
    if (!sim::isFinite(centerMeters) || !sim::isFinite(halfExtentsMeters) ||
        halfExtentsMeters.x <= 0.0 || halfExtentsMeters.y <= 0.0 ||
        halfExtentsMeters.z <= 0.0) {
        throw std::invalid_argument(
            "overlay box requires a finite center and positive extents");
    }
    if (mesh.vertices.size() >
        static_cast<std::size_t>(
            std::numeric_limits<std::uint32_t>::max() - 24U)) {
        throw std::overflow_error("overlay geometry vertex index overflow");
    }

    struct Face {
        sim::Vec3 normal;
        sim::Vec3 horizontal;
        sim::Vec3 vertical;
    };
    const double x = halfExtentsMeters.x;
    const double y = halfExtentsMeters.y;
    const double z = halfExtentsMeters.z;
    const Face faces[6] = {
        {{1.0, 0.0, 0.0}, {0.0, 0.0, -z}, {0.0, y, 0.0}},
        {{-1.0, 0.0, 0.0}, {0.0, 0.0, z}, {0.0, y, 0.0}},
        {{0.0, 1.0, 0.0}, {x, 0.0, 0.0}, {0.0, 0.0, -z}},
        {{0.0, -1.0, 0.0}, {x, 0.0, 0.0}, {0.0, 0.0, z}},
        {{0.0, 0.0, 1.0}, {x, 0.0, 0.0}, {0.0, y, 0.0}},
        {{0.0, 0.0, -1.0}, {-x, 0.0, 0.0}, {0.0, y, 0.0}},
    };

    for (const Face& face : faces) {
        const std::uint32_t base =
            static_cast<std::uint32_t>(mesh.vertices.size());
        const sim::Vec3 faceCenter{
            centerMeters.x + face.normal.x * x,
            centerMeters.y + face.normal.y * y,
            centerMeters.z + face.normal.z * z,
        };
        const double corners[4][2] = {{-1.0, -1.0}, {1.0, -1.0},
                                      {1.0, 1.0}, {-1.0, 1.0}};
        for (const auto& corner : corners) {
            mesh.vertices.push_back({
                .position = {
                    checkedOverlayFloat(faceCenter.x +
                                        face.horizontal.x * corner[0] +
                                        face.vertical.x * corner[1]),
                    checkedOverlayFloat(faceCenter.y +
                                        face.horizontal.y * corner[0] +
                                        face.vertical.y * corner[1]),
                    checkedOverlayFloat(faceCenter.z +
                                        face.horizontal.z * corner[0] +
                                        face.vertical.z * corner[1]),
                },
                .normal = {
                    static_cast<float>(face.normal.x),
                    static_cast<float>(face.normal.y),
                    static_cast<float>(face.normal.z),
                },
                .color = color,
            });
        }
        mesh.indices.insert(
            mesh.indices.end(),
            {base, base + 1U, base + 2U, base, base + 2U, base + 3U});
    }
}

}  // namespace konbini::render::detail
