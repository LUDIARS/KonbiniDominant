#pragma once
#include "konbini/render/world_mesh.h"
#include "konbini/sim/render_snapshot.h"
namespace konbini::render {
[[nodiscard]] WorldMesh buildPhase1OverlayGeometry(const sim::RenderSnapshot& snapshot);
}  // namespace konbini::render
