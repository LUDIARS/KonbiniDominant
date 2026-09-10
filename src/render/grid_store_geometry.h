#pragma once
#include "konbini/render/store_marker_geometry.h"
namespace konbini::render::detail {
WorldMesh buildGridStoreGeometry(std::span<const sim::RenderStore> stores, const StoreMarkerSpec& spec);
void appendGridStoreSign(WorldMesh& mesh, const sim::RenderStore& first, std::size_t length,
                         const StoreMarkerSpec& spec);
}
