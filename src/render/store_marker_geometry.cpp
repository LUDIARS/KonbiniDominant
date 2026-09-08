#include "konbini/render/store_marker_geometry.h"

#include <cmath>
#include <cstddef>
#include <stdexcept>

#include "storefront_geometry.h"

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
            !sim::isFirstPlayableChainId(store.chain)) {
            throw std::invalid_argument(
                "store marker geometry received an invalid render store");
        }
        detail::appendStorefront(mesh, store, spec);
    }
    return mesh;
}

}  // namespace konbini::render
