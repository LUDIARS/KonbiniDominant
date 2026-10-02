#pragma once

#include <cstddef>

#include "konbini/adapters/pictor/gpu_asset_store.h"

// @implements spec/interface/pictor-rendering.md Presentation objects

namespace konbini::adapters::pictor {

struct PresentationGeometryLoadReport {
    std::size_t uploaded = 0;
};

// Uploads every shared presentation mesh (`render::buildPresentationMeshes`)
// once: the resident Visia, the baked landing ring frames, the bubble quad /
// tail and the speech glyph atlas. Called at startup next to the city
// geometry, never inside the frame loop. A second call is `std::logic_error`
// (the asset store rejects duplicate keys).
[[nodiscard]] PresentationGeometryLoadReport loadPresentationGeometry(
    GpuAssetStore& assets);

}  // namespace konbini::adapters::pictor
