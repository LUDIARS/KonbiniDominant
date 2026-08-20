#pragma once

#include <array>
#include <type_traits>

namespace konbini::render {

struct WorldVertex {
    // presentation 色の型は palette / geometry builder / GPU adapter が共有
    // するので、vertex 側に 1 つ置いて各所での再宣言を避ける。
    using Position = std::array<float, 3>;
    using Normal = std::array<float, 3>;
    using ColorRgba = std::array<float, 4>;

    Position position{};
    Normal normal{};
    ColorRgba color{1.0F, 1.0F, 1.0F, 1.0F};
};

static_assert(std::is_standard_layout_v<WorldVertex>);
static_assert(sizeof(WorldVertex) == sizeof(float) * 10);

}  // namespace konbini::render
