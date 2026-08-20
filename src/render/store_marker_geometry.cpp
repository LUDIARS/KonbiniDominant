#include "konbini/render/store_marker_geometry.h"

#include <cmath>
#include <stdexcept>

#include "konbini/render/world_palette.h"
#include "world_box_geometry.h"

// @implements spec/interface/pictor-rendering.md Game-owned render domain

namespace konbini::render {

namespace {

void validateSpec(const StoreMarkerSpec& spec) {
    if (!std::isfinite(spec.halfWidthMeters) ||
        !std::isfinite(spec.heightMeters) || spec.halfWidthMeters <= 0.0 ||
        spec.heightMeters <= 0.0 || !std::isfinite(spec.alpha) ||
        spec.alpha <= 0.0F || spec.alpha > 1.0F) {
        throw std::invalid_argument("invalid store marker spec");
    }
}

}  // namespace

StoreMarkerSpec defaultStoreMarkerSpec() noexcept {
    return {};
}

// @implements spec/interface/pictor-rendering.md Game-owned render domain
WorldMesh buildStoreMarkerGeometry(
    const std::span<const sim::RenderStore> stores,
    const StoreMarkerSpec& spec) {
    validateSpec(spec);

    WorldMesh mesh;
    mesh.vertices.reserve(stores.size() * 24U);
    mesh.indices.reserve(stores.size() * 36U);
    for (const sim::RenderStore& store : stores) {
        if (!store.id.isValid() || !store.facilityId.isValid() ||
            !sim::isFinite(store.positionMeters) ||
            !sim::isFirstPlayableChainId(store.chain)) {
            throw std::invalid_argument(
                "store marker geometry received an invalid render store");
        }
        // store position は接地点なので、box 中心を高さの半分だけ持ち上げる。
        const sim::Vec3 center{
            store.positionMeters.x,
            store.positionMeters.y + spec.heightMeters * 0.5,
            store.positionMeters.z,
        };
        detail::appendAxisAlignedBox(
            mesh, center,
            {spec.halfWidthMeters, spec.heightMeters * 0.5,
             spec.halfWidthMeters},
            chainColor(store.chain, spec.alpha));
    }
    return mesh;
}

}  // namespace konbini::render
