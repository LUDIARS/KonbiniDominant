#pragma once

#include <optional>
#include <span>

#include "konbini/render/world_ray.h"
#include "konbini/sim/render_snapshot.h"

namespace konbini::render {

struct FacilityPick {
    sim::FigmentumFacilityKey figmentumKey{};
    sim::FacilityId facilityId{};
    double distanceMeters = 0.0;
};

[[nodiscard]] std::optional<FacilityPick> pickFacility(
    const WorldRay& ray, std::span<const sim::RenderFacility> facilities);

}  // namespace konbini::render
