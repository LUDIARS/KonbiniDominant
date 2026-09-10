#pragma once
#include <array>
#include <span>
#include <vector>
#include "konbini/sim/phase1_content.h"
#include "konbini/sim/store_table.h"
namespace konbini::sim {
// Canonical vertex order is StoreId order, independent of mesh winding.
struct DominantTriangle {
    std::array<StoreId, 3> stores{};
    std::array<Vec3, 3> points{};
    ChainId chain = ChainId::Losan;
    std::uint32_t dimension = 0;
};
[[nodiscard]] bool triangleContains(const DominantTriangle& triangle, Vec3 point) noexcept;
[[nodiscard]] std::vector<DominantTriangle> buildDominantTriangles(
    const StoreTable& stores, const Phase1Content& rules);
void applyTriangleRevenue(StoreTable& stores,
                          std::span<const DominantTriangle> triangles,
                          const Phase1Content& rules);
}  // namespace konbini::sim
