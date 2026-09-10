#pragma once
#include "konbini/sim/render_snapshot.h"
#include "konbini/render/world_mesh.h"
namespace konbini::render {
WorldMesh buildSkillPulseGeometry(const sim::RenderSnapshot& snapshot,
                                  std::span<const sim::RenderStore> visibleStores);
}
