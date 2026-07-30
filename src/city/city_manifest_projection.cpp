#include "konbini/city/city_manifest_projection.h"

// @implements spec/data/world-state.md `FacilityTable`

namespace konbini::city {

// 未検証の manifest から table を作らない。ここを通った row は
// FacilityTable::append の事前条件 (有効 ID / finite bounds) を満たす。
// @implements spec/data/world-state.md `FacilityTable`
sim::FacilityTable projectFacilityTable(const CityManifest& manifest,
                                        const std::uint32_t dimension) {
    validateCityManifest(manifest);

    sim::FacilityTable table;
    for (const ManifestFacility& facility : manifest.facilities) {
        table.append({
            .id = facility.id,
            .figmentumKey = facility.figmentumKey,
            .dimension = dimension,
            .positionMeters = facility.recipe.originMeters,
            .boundsMeters = facility.boundsMeters,
            .isBuildable = facility.isBuildable,
            .state = sim::FacilityState::Intact,
        });
    }
    return table;
}

}  // namespace konbini::city
