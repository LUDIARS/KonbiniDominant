#include "konbini/adapters/pictor/world_geometry_loader.h"

#include <stdexcept>
#include "konbini/city/grid_town.h"

#include "konbini/render/facility_world_mesh.h"

// @implements spec/interface/pictor-rendering.md Geometry conversion

namespace konbini::adapters::pictor {

WorldGeometryLoadReport loadCityGeometry(
    const city::GeneratedCity& city, WorldGeometryCache& cache) {
    if (!cache.isInitialized()) {
        throw std::logic_error(
            "world geometry cache must be initialized before loading a city");
    }
    if (city::isGridTown(city.manifest)) {
        city::validateCityManifest(city.manifest);
        if (!city.geometry.empty()) throw std::invalid_argument("grid town must not contain buildings");
        return {};
    }
    if (city.geometry.empty()) {
        throw std::invalid_argument(
            "generated city has no facility geometry to upload");
    }

    WorldGeometryLoadReport report;
    for (const std::shared_ptr<const city::FacilityGeometry>& geometry :
         city.geometry) {
        if (geometry == nullptr) {
            throw std::invalid_argument(
                "generated city contains a null facility geometry");
        }
        if (cache.contains(geometry->figmentumKey)) {
            ++report.deduplicated;
            continue;
        }
        cache.insert(
            geometry->figmentumKey,
            render::buildFacilityWorldMesh(*geometry));
        ++report.uploaded;
    }
    return report;
}

}  // namespace konbini::adapters::pictor
