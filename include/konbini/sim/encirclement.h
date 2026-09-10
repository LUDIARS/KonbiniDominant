#pragma once
#include <array>
#include "konbini/sim/dominant_triangle.h"
namespace konbini::sim {
// Losing the enclosing triangle resets its warning, even if another takes over.
struct Encirclement {
    StoreId target{};
    ChainId attacker = ChainId::Losan;
    std::array<StoreId, 3> triangleStores{};
    std::uint32_t elapsedTicks = 0;
};
}  // namespace konbini::sim
