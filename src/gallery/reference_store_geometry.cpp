#include "reference_store_geometry.h"
#include <stdexcept>
#include "../render/world_box_geometry.h"

// @implements spec/feature/three-store-brands.md Current Fable comparison
namespace konbini::gallery {
render::WorldMesh buildReferenceStores(std::span<const sim::RenderStore> stores) {
    render::WorldMesh mesh;
    for (const auto& store : stores) {
        // Exact marker dimensions, alpha, and chain colors from current main
        // (4678511), identified by the user as the Fable implementation.
        render::WorldVertex::ColorRgba color;
        switch (store.chain) {
            case sim::ChainId::Losan: color = {0.18F, 0.52F, 0.96F, 0.85F}; break;
            case sim::ChainId::Famoma: color = {0.94F, 0.27F, 0.25F, 0.85F}; break;
            case sim::ChainId::SebanIleban: color = {0.20F, 0.78F, 0.43F, 0.85F}; break;
            default: throw std::invalid_argument("unknown reference chain");
        }
        render::detail::appendAxisAlignedBox(mesh,
            {store.positionMeters.x, store.positionMeters.y + 3.5, store.positionMeters.z},
            {3.0, 3.5, 3.0}, color);
    }
    return mesh;
}
}
