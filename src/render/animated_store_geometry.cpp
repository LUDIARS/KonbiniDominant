// @implements spec/feature/store-construction-effects.md Connected stores
#include "konbini/render/animated_store_geometry.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>
namespace konbini::render {
namespace {
void appendTransformed(WorldMesh& target, WorldMesh source,
                       const StoreConstructionVisual& visual) {
    const auto& origin = visual.store.positionMeters;
    const auto& pose = visual.animation.storePose;
    const double angle = pose.yawDegrees * std::numbers::pi / 180.0;
    const double c = std::cos(angle), s = std::sin(angle);
    const auto base = static_cast<std::uint32_t>(target.vertices.size());
    for (auto vertex : source.vertices) {
        const double x = vertex.position[0] - origin.x;
        const double z = vertex.position[2] - origin.z;
        vertex.position[0] = static_cast<float>(pose.positionMeters.x + c*x + s*z);
        vertex.position[1] += static_cast<float>(pose.positionMeters.y - origin.y);
        vertex.position[2] = static_cast<float>(pose.positionMeters.z - s*x + c*z);
        const double nx = vertex.normal[0], nz = vertex.normal[2];
        vertex.normal[0] = static_cast<float>(c*nx + s*nz);
        vertex.normal[2] = static_cast<float>(-s*nx + c*nz);
        target.vertices.push_back(vertex);
    }
    for (const auto index : source.indices) target.indices.push_back(base + index);
}
}
WorldMesh buildAnimatedStoreGeometry(
    std::span<const sim::RenderStore> stores, const StoreMarkerSpec& spec,
    std::span<const StoreConstructionVisual> construction) {
    std::vector<sim::RenderStore> settled;
    for (const auto& store : stores) {
        const bool airborne = std::ranges::any_of(construction, [&](const auto& item) {
            return item.store.id == store.id && !item.animation.hasLanded;
        });
        if (!airborne) settled.push_back(store);
    }
    auto mesh = buildStoreMarkerGeometry(settled, spec);
    for (const auto& visual : construction) {
        if (visual.animation.hasLanded || std::ranges::none_of(stores, [&](const auto& store) {
                return store.id == visual.store.id;
            })) continue;
        // An airborne cell is a complete independent storefront. At impact it
        // joins the settled group so neighboring facades and the sign extend.
        appendTransformed(mesh, buildStoreMarkerGeometry({&visual.store, 1}, spec), visual);
    }
    return mesh;
}
}
