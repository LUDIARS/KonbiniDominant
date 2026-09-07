#pragma once
#include <span>
#include "konbini/render/world_mesh.h"
#include "konbini/sim/render_snapshot.h"
namespace konbini::gallery {
// Frozen current-main reference, commit 4678511. Do not use the new palette.
render::WorldMesh buildReferenceStores(std::span<const sim::RenderStore> stores);
}
