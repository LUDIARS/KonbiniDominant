#include "konbini/adapters/pictor/presentation_geometry_loader.h"

#include <stdexcept>

#include "konbini/adapters/pictor/gpu_mesh_key.h"
#include "konbini/render/presentation_meshes.h"

// @implements spec/interface/pictor-rendering.md Presentation objects

namespace konbini::adapters::pictor {

// @implements spec/interface/pictor-rendering.md Presentation objects
PresentationGeometryLoadReport loadPresentationGeometry(GpuAssetStore& assets) {
    if (!assets.isInitialized()) {
        throw std::logic_error(
            "GPU asset store must be initialized before presentation meshes");
    }
    PresentationGeometryLoadReport report;
    for (const render::PresentationMesh& mesh :
         render::buildPresentationMeshes()) {
        static_cast<void>(
            assets.insert(GpuMeshKey::presentation(mesh.key), mesh.mesh));
        ++report.uploaded;
    }
    return report;
}

}  // namespace konbini::adapters::pictor
