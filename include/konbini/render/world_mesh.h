#pragma once

#include <cstdint>
#include <vector>

#include "konbini/render/world_vertex.h"

// @implements spec/interface/pictor-rendering.md Game-owned render domain

namespace konbini::render {

// game-owned な CPU mesh。`WorldVertex` と同じく Pictor / Vulkan header へ
// 依存しないので、simulation 由来の geometry も Figmentum 由来の geometry も
// 同じ型で GPU adapter へ渡せる。overlay 系 builder が各自 vertex/index の
// 組を宣言し直すと vertex 型の重複を招くため、この 1 型に集約する。
// @implements spec/interface/pictor-rendering.md Geometry conversion
struct WorldMesh {
    std::vector<WorldVertex> vertices;
    std::vector<std::uint32_t> indices;
};

}  // namespace konbini::render
