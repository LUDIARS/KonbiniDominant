#pragma once

#include <cstdint>
#include <vector>

#include "konbini/render/visia.h"
#include "konbini/render/world_vertex.h"

// @implements spec/interface/visia-presentation.md CPU primitive geometry
// @implements spec/feature/npc-conversations-and-placement-feedback.md Visia dummy

namespace konbini::render {

struct VisiaGeometry {
    std::vector<WorldVertex> vertices;
    std::vector<std::uint32_t> indices;
};

// Resolve a validated game-owned Visia instance to its current primitive
// dummy. The ID selects the definition explicitly; unknown IDs and fields
// that are meaningless for that definition are rejected.
[[nodiscard]] VisiaGeometry buildPrimitiveVisiaGeometry(
    const VisiaInstance& instance);

// The resident pose is measured from the center of its feet. Yaw is applied
// to both the body and head geometry before the world translation.
[[nodiscard]] VisiaGeometry buildResidentPrimitiveGeometry(
    const ResidentPrimitiveVisiaDefinition& definition,
    const VisiaPose& pose);

// normalizedAge is inclusive [0, 1]. Radius expands and alpha fades as age
// advances; invalid ages or definitions are rejected.
[[nodiscard]] VisiaGeometry buildStoreLandingEffectPrimitiveGeometry(
    const StoreLandingEffectPrimitiveVisiaDefinition& definition,
    const sim::Vec3& originMeters,
    double normalizedAge);

}  // namespace konbini::render
