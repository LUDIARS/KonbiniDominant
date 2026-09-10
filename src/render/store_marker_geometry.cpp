#include "konbini/render/store_marker_geometry.h"

#include <cmath>
#include <cstddef>
#include <stdexcept>

#include "storefront_geometry.h"
#include "grid_store_geometry.h"
#include "world_box_geometry.h"
#include "konbini/render/world_palette.h"

// @implements spec/interface/pictor-rendering.md Game-owned render domain

namespace konbini::render {

namespace {

void validateSpec(const StoreMarkerSpec& spec) {
    if (!std::isfinite(spec.halfWidthMeters) ||
        !std::isfinite(spec.heightMeters) || spec.halfWidthMeters <= 0.0 ||
        spec.heightMeters <= 0.0 || !std::isfinite(spec.alpha) ||
        spec.alpha <= 0.0F || spec.alpha > 1.0F || !std::isfinite(spec.gridCellMeters) || spec.gridCellMeters < 0) {
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
    if (spec.gridCellMeters > 0) return detail::buildGridStoreGeometry(stores,spec);

    WorldMesh mesh;
    // A storefront is ~30 structural boxes (shell, glazing, door, canopy) plus
    // one box per lit 5x7 pixel of the brand name; the worst case, MOONPANTRY,
    // lights 156 pixels, so ~190 boxes. Rounded up to leave headroom for sign
    // or facade edits. Reserving one box per store, as the old single-box
    // marker did, would force a growth cascade per facade.
    constexpr std::size_t kBoxesPerStorefront = 200U;
    mesh.vertices.reserve(stores.size() * kBoxesPerStorefront * 24U);
    mesh.indices.reserve(stores.size() * kBoxesPerStorefront * 36U);
    for (const sim::RenderStore& store : stores) {
        if (!store.id.isValid() || !store.facilityId.isValid() ||
            !sim::isFinite(store.positionMeters) ||
            !sim::isSimulationChainId(store.chain)) {
            throw std::invalid_argument(
                "store marker geometry received an invalid render store");
        }
        // store position は接地点なので、box 中心を高さの半分だけ持ち上げる。
        const sim::Vec3 center{
            store.positionMeters.x,
            store.positionMeters.y + spec.heightMeters * 0.5,
            store.positionMeters.z,
        };
        if (store.isAntiStore || store.chain == sim::ChainId::Aion) {
            detail::appendAxisAlignedBox(
                mesh, center,
                {spec.halfWidthMeters, spec.heightMeters * 0.5,
                 spec.halfWidthMeters},
                store.isAntiStore ? WorldColor{0.92F,0.12F,0.85F,spec.alpha} : chainColor(store.chain, spec.alpha));
        } else {
            detail::appendStorefront(mesh, store, spec);
        }
        if (store.faith == 100 || store.chain == sim::ChainId::Aion) {
            const auto crown = sim::Vec3{center.x, store.positionMeters.y + spec.heightMeters + 0.25, center.z};
            const auto form = store.id.value().index % 7;
            const double width = store.chain == sim::ChainId::Aion ? 0.45 + 0.08 * form : 0.7;
            const double depth = store.chain == sim::ChainId::Aion ? 0.95 - 0.07 * form : 0.7;
            detail::appendAxisAlignedBox(mesh,crown,{spec.halfWidthMeters*width,0.25,spec.halfWidthMeters*depth},
                store.chain == sim::ChainId::Aion ? WorldColor{0.9F,0.6F,1.0F,spec.alpha} : WorldColor{1.0F,0.83F,0.2F,spec.alpha});
        }
    }
    return mesh;
}

}  // namespace konbini::render
