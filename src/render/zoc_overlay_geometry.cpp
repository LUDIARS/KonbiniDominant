#include "konbini/render/zoc_overlay_geometry.h"

#include <cmath>
#include <limits>
#include <stdexcept>

#include "konbini/render/world_palette.h"

// @implements spec/interface/pictor-rendering.md Game-owned render domain

namespace konbini::render {

namespace {

constexpr double kTwoPi =
    6.283185307179586476925286766559005768;

float checkedFloat(const double value) {
    constexpr double kFloatMax =
        static_cast<double>(std::numeric_limits<float>::max());
    if (!std::isfinite(value) || value < -kFloatMax || value > kFloatMax) {
        throw std::invalid_argument(
            "ZOC coordinate cannot be represented as float");
    }
    return static_cast<float>(value);
}

}  // namespace

// store ごとに中心 1 + rim (segmentCount + 1) の triangle fan を出す。rim は
// 角度 0 と 2π を別 vertex に持たせ、seam の頂点共有で index 順が
// segment 依存に分岐しないようにする。winding は +Y から見て CCW で、
// vertex normal (0, 1, 0) と表裏が一致する。
// @implements spec/interface/pictor-rendering.md Game-owned render domain
ZocOverlayGeometry buildZocOverlayGeometry(
    const std::span<const sim::RenderStore> stores,
    const std::uint32_t segmentCount, const float groundYMeters) {
    if (segmentCount < 3 || !std::isfinite(groundYMeters)) {
        throw std::invalid_argument(
            "ZOC overlay requires at least three segments and finite height");
    }
    const std::size_t verticesPerStore =
        static_cast<std::size_t>(segmentCount) + 2;
    const std::size_t indicesPerStore =
        static_cast<std::size_t>(segmentCount) * 3;
    if (stores.size() >
        static_cast<std::size_t>(
            std::numeric_limits<std::uint32_t>::max()) /
            verticesPerStore ||
        stores.size() >
            static_cast<std::size_t>(
                std::numeric_limits<std::uint32_t>::max()) /
                indicesPerStore) {
        throw std::overflow_error(
            "ZOC overlay exceeds 32-bit geometry space");
    }

    ZocOverlayGeometry geometry;
    geometry.vertices.reserve(stores.size() * verticesPerStore);
    geometry.indices.reserve(
        stores.size() * indicesPerStore);
    for (const sim::RenderStore& store : stores) {
        if (!store.id.isValid() || !store.facilityId.isValid() ||
            !sim::isFinite(store.positionMeters) ||
            !std::isfinite(store.zocRadiusMeters) ||
            store.zocRadiusMeters <= 0.0 ||
            !sim::isFirstPlayableChainId(store.chain)) {
            throw std::invalid_argument(
                "ZOC overlay received an invalid render store");
        }
        const WorldColor color = chainColor(store.chain, 0.24F);
        const std::uint32_t base =
            static_cast<std::uint32_t>(geometry.vertices.size());
        geometry.vertices.push_back({
            .position = {checkedFloat(store.positionMeters.x),
                         groundYMeters,
                         checkedFloat(store.positionMeters.z)},
            .normal = {0.0F, 1.0F, 0.0F},
            .color = color,
        });
        for (std::uint32_t segment = 0; segment <= segmentCount;
             ++segment) {
            const double angle =
                kTwoPi * static_cast<double>(segment) /
                static_cast<double>(segmentCount);
            geometry.vertices.push_back({
                .position = {
                    checkedFloat(store.positionMeters.x +
                                 std::cos(angle) *
                                     store.zocRadiusMeters),
                    groundYMeters,
                    checkedFloat(store.positionMeters.z +
                                 std::sin(angle) *
                                     store.zocRadiusMeters),
                },
                .normal = {0.0F, 1.0F, 0.0F},
                .color = color,
            });
        }
        for (std::uint32_t segment = 0; segment < segmentCount;
             ++segment) {
            geometry.indices.push_back(base);
            geometry.indices.push_back(base + segment + 2);
            geometry.indices.push_back(base + segment + 1);
        }
    }
    return geometry;
}

}  // namespace konbini::render
