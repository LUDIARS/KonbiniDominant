#pragma once

#include <cstdint>

#include "konbini/render/store_marker_geometry.h"

namespace konbini::render {

struct StoreTowerSpec {
    // Exactly thirty storefront floors, alternating the three original brands.
    static constexpr std::uint32_t floorCount = 30;
    StoreMarkerSpec floor{6.0, 3.6, 1.0F};
};

// A presentation asset, not thirty simulated stores or new economic rules.
// Origin is the bottom floor's ground contact; total height is 30 * floor height.
[[nodiscard]] WorldMesh buildStoreTowerGeometry(
    sim::Vec3 origin, const StoreTowerSpec& spec = {});

}  // namespace konbini::render
