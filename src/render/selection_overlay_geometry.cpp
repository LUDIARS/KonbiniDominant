#include "konbini/render/selection_overlay_geometry.h"

#include <cmath>
#include <stdexcept>

#include "konbini/render/world_palette.h"
#include "konbini/render/facility_display_bounds.h"
#include "world_box_geometry.h"

// @implements spec/interface/pictor-rendering.md Game-owned render domain

namespace konbini::render {

SelectionOverlaySpec defaultSelectionOverlaySpec() noexcept {
    return {};
}

// @implements spec/interface/pictor-rendering.md Game-owned render domain
WorldMesh buildSelectionOverlayGeometry(
    const sim::RenderFacility& facility, const SelectionOverlaySpec& spec) {
    if (!std::isfinite(spec.marginMeters) || spec.marginMeters <= 0.0) {
        throw std::invalid_argument(
            "selection overlay requires a positive margin");
    }
    if (!facility.id.isValid() || !facility.figmentumKey.isValid() ||
        !sim::isFiniteAndOrdered(facility.boundsMeters)) {
        throw std::invalid_argument(
            "selection overlay received an invalid render facility");
    }

    const auto bounds = facilityDisplayBounds(facility);
    const sim::Vec3& min = bounds.min;
    const sim::Vec3& max = bounds.max;
    WorldMesh mesh;
    mesh.vertices.reserve(24);
    mesh.indices.reserve(36);
    detail::appendAxisAlignedBox(
        mesh,
        {(min.x + max.x) * 0.5, (min.y + max.y) * 0.5,
         (min.z + max.z) * 0.5},
        {(max.x - min.x) * 0.5 + spec.marginMeters,
         (max.y - min.y) * 0.5 + spec.marginMeters,
         (max.z - min.z) * 0.5 + spec.marginMeters},
        selectedFacilityColor(spec.alpha));
    return mesh;
}

}  // namespace konbini::render
