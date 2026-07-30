#pragma once

#include <array>
#include <type_traits>

namespace konbini::render {

struct WorldVertex {
    std::array<float, 3> position{};
    std::array<float, 3> normal{};
    std::array<float, 4> color{1.0F, 1.0F, 1.0F, 1.0F};
};

static_assert(std::is_standard_layout_v<WorldVertex>);
static_assert(sizeof(WorldVertex) == sizeof(float) * 10);

}  // namespace konbini::render
