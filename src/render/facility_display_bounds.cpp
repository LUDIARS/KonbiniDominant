#include "konbini/render/facility_display_bounds.h"
#include "konbini/render/store_marker_geometry.h"
#include "konbini/city/grid_town.h"
namespace konbini::render {
sim::Bounds3 facilityDisplayBounds(const sim::RenderFacility& facility) {
    if (!facility.isLotRepresentation) { return facility.boundsMeters; }
    if (facility.boundsMeters.max.y == 0.2 && facility.boundsMeters.min.y == 0 &&
        facility.boundsMeters.max.x-facility.boundsMeters.min.x == city::kGridCellMeters &&
        facility.boundsMeters.max.z-facility.boundsMeters.min.z == city::kGridCellMeters) {
        auto bounds = facility.boundsMeters;
        bounds.min.y = facility.positionMeters.y;
        bounds.max.y = bounds.min.y + 2.8;
        return bounds;
    }
    const auto marker = defaultStoreMarkerSpec();
    const bool destroyed = facility.state == sim::FacilityState::Destroyed;
    const double half = destroyed ? kDestroyedLotHalfWidthMeters : marker.halfWidthMeters;
    const double height = destroyed ? kDestroyedLotHeightMeters : marker.heightMeters;
    const auto p = facility.positionMeters;
    return {{p.x-half, p.y, p.z-half}, {p.x+half, p.y+height, p.z+half}};
}
}  // namespace konbini::render
