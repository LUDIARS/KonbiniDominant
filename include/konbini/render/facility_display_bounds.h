#pragma once
#include "konbini/sim/render_snapshot.h"
namespace konbini::render {
inline constexpr double kDestroyedLotHalfWidthMeters = 3.5;
inline constexpr float kDestroyedLotHeightMeters = 0.14F;
// Share the visible marker's bounds between picking and selection highlighting.
[[nodiscard]] sim::Bounds3 facilityDisplayBounds(const sim::RenderFacility& facility);
}  // namespace konbini::render
