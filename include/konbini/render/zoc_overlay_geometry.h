#pragma once

#include <cstdint>
#include <span>

#include "konbini/render/world_mesh.h"
#include "konbini/sim/render_snapshot.h"

namespace konbini::render {

// ZOC overlay も他の world geometry と同じ vertex/index の組なので、専用
// struct を持たず `WorldMesh` を使う。名前は呼び出し側の意図を残すための
// alias で、型としては world geometry 全体と同一。
using ZocOverlayGeometry = WorldMesh;

[[nodiscard]] ZocOverlayGeometry buildZocOverlayGeometry(
    std::span<const sim::RenderStore> stores,
    std::uint32_t segmentCount = 48, float groundYMeters = 0.03F);

}  // namespace konbini::render
