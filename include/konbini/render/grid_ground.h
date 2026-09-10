// @implements spec/feature/grid-town-and-vector-ui.md Grid town
#pragma once
#include "konbini/render/facility_picker.h"
#include "konbini/render/world_mesh.h"

namespace konbini::render {
[[nodiscard]] WorldMesh buildGridGround(std::span<const sim::RenderFacility> cells);
[[nodiscard]] std::optional<FacilityPick> pickGridCell(
    const WorldRay& ray, std::span<const sim::RenderFacility> cells, double floorYMeters);
}
