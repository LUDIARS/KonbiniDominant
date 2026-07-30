#pragma once

#include "figmentum/gen/city_plan.h"

#include "konbini/city/city_manifest.h"
#include "konbini/sim/entity_id.h"

namespace konbini::adapters::figmentum {

[[nodiscard]] city::CityManifest projectCityPlan(
    const fg::CityPlan& plan,
    sim::GenerationalIdPool<sim::FacilityId>& facilityIds);

}  // namespace konbini::adapters::figmentum
