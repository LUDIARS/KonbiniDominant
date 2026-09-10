#pragma once
#include <span>
#include "konbini/render/store_construction_visual.h"
#include "konbini/render/store_marker_geometry.h"
namespace konbini::render {
[[nodiscard]] WorldMesh buildAnimatedStoreGeometry(
    std::span<const sim::RenderStore> visibleStores, const StoreMarkerSpec& spec,
    std::span<const StoreConstructionVisual> construction);
}
