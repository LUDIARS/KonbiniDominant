#pragma once

#include "figmentum/gen/city_plan.h"

#include "konbini/city/city_manifest.h"
#include "konbini/sim/entity_id.h"

namespace konbini::adapters::figmentum {

// `plan` must be `planCity(params, plan.seed)`: the pedestrian path contract is
// derived from the same params and projected into the manifest.
[[nodiscard]] city::CityManifest projectCityPlan(
    const fg::CityPlanParams& params, const fg::CityPlan& plan,
    sim::GenerationalIdPool<sim::FacilityId>& facilityIds);

}  // namespace konbini::adapters::figmentum
