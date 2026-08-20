#include "konbini/render/facility_world_mesh.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

// @implements spec/interface/pictor-rendering.md Geometry conversion

namespace konbini::render {

namespace {

[[nodiscard]] float checkedFloat(const double value) {
    constexpr double kMaximum =
        static_cast<double>(std::numeric_limits<float>::max());
    if (!std::isfinite(value) || value < -kMaximum || value > kMaximum) {
        throw std::invalid_argument(
            "facility geometry value cannot be represented as float");
    }
    return static_cast<float>(value);
}

// marching-cubes は隣接 SDF 値が同符号の cell で退化 normal を出しうる。
// 0 ベクトルは shader 側が「陰影なし」として扱う既知の値なので通し、
// 非有限値だけを弾く。
[[nodiscard]] WorldVertex::Normal toVertexNormal(const sim::Vec3 normal) {
    const double lengthSquared = normal.x * normal.x + normal.y * normal.y +
                                 normal.z * normal.z;
    if (!std::isfinite(lengthSquared)) {
        throw std::invalid_argument(
            "facility geometry has a non-finite normal");
    }
    if (lengthSquared <= 0.0) {
        return {0.0F, 0.0F, 0.0F};
    }
    const double length = std::sqrt(lengthSquared);
    return {
        checkedFloat(normal.x / length),
        checkedFloat(normal.y / length),
        checkedFloat(normal.z / length),
    };
}

}  // namespace

// @implements spec/interface/pictor-rendering.md Geometry conversion
WorldMesh buildFacilityWorldMesh(const city::FacilityGeometry& geometry) {
    if (!geometry.figmentumKey.isValid()) {
        throw std::invalid_argument(
            "facility geometry has no stable Figmentum key");
    }
    const std::size_t vertexCount = geometry.positionsMeters.size();
    if (vertexCount == 0 || geometry.normals.size() != vertexCount ||
        geometry.indices.empty() || geometry.indices.size() % 3U != 0U) {
        throw std::invalid_argument(
            "facility geometry is not a non-empty indexed triangle list");
    }
    if (vertexCount >
        static_cast<std::size_t>(
            std::numeric_limits<std::uint32_t>::max())) {
        throw std::overflow_error(
            "facility geometry exceeds 32-bit index space");
    }

    WorldMesh mesh;
    mesh.vertices.reserve(vertexCount);
    for (std::size_t vertex = 0; vertex < vertexCount; ++vertex) {
        const sim::Vec3 position = geometry.positionsMeters[vertex];
        if (!sim::isFinite(position)) {
            throw std::invalid_argument(
                "facility geometry has a non-finite position");
        }
        mesh.vertices.push_back({
            .position = {checkedFloat(position.x), checkedFloat(position.y),
                         checkedFloat(position.z)},
            .normal = toVertexNormal(geometry.normals[vertex]),
            .color = kFacilityNeutralVertexColor,
        });
    }

    // 範囲外 index は GPU では未定義動作にしかならないので、upload 前の
    // ここで落とす。
    const std::uint32_t vertexLimit =
        static_cast<std::uint32_t>(vertexCount);
    mesh.indices.reserve(geometry.indices.size());
    for (const std::uint32_t index : geometry.indices) {
        if (index >= vertexLimit) {
            throw std::out_of_range(
                "facility geometry index is outside the vertex range");
        }
        mesh.indices.push_back(index);
    }
    return mesh;
}

}  // namespace konbini::render
