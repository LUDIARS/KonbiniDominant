#pragma once

#include "konbini/render/store_tower_geometry.h"

namespace konbini::render::detail {

// Dense, deterministic service annexes surrounding the thirty store floors.
void appendStoreTowerAdditions(WorldMesh& mesh, sim::Vec3 origin,
                               const StoreTowerSpec& spec);

}  // namespace konbini::render::detail
