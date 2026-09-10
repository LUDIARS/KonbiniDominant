#pragma once
#include "konbini/render/store_placement_animation.h"
#include "konbini/sim/render_snapshot.h"
namespace konbini::render {
// Presentation-only state; never serialized or written back to simulation.
struct StoreConstructionVisual {
    sim::RenderStore store;
    StorePlacementAnimationSample animation;
    double elapsedSeconds = 0.0;
};
}
