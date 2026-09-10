// @implements spec/feature/grid-town-and-vector-ui.md Square stores
#pragma once
#include <array>
#include <optional>
#include <span>
#include <vector>
#include "konbini/sim/render_snapshot.h"

namespace konbini::render {
enum GridStoreSide : std::size_t { Left, Right, Back, Front };
using GridStoreLinks = std::array<std::optional<std::size_t>, 4>;
[[nodiscard]] std::vector<GridStoreLinks> connectGridStores(
    std::span<const sim::RenderStore> stores, double cellMeters);
}
