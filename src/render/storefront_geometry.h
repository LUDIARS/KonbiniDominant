#pragma once

#include "konbini/render/store_marker_geometry.h"

namespace konbini::render::detail {

// Appends one game-scale storefront. The snapshot remains simulation-owned.
void appendStorefront(WorldMesh& mesh, const sim::RenderStore& store,
                      const StoreMarkerSpec& spec);

}  // namespace konbini::render::detail
