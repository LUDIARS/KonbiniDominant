#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "konbini/render/world_vertex.h"
#include "konbini/sim/render_snapshot.h"

namespace konbini::render {

struct ZocOverlayGeometry {
    std::vector<WorldVertex> vertices;
    std::vector<std::uint32_t> indices;
};

[[nodiscard]] ZocOverlayGeometry buildZocOverlayGeometry(
    std::span<const sim::RenderStore> stores,
    std::uint32_t segmentCount = 48, float groundYMeters = 0.03F);

}  // namespace konbini::render
