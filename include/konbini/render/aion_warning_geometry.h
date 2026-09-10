#pragma once
#include "konbini/render/hud_text_geometry.h"
#include "konbini/sim/render_snapshot.h"
namespace konbini::render {
WorldMesh buildAionWarningGeometry(const sim::HudViewModel& hud,ViewportExtent extent);
}
