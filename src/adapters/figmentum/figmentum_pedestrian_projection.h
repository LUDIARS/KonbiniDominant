#pragma once

#include "figmentum/gen/city_plan.h"
#include "figmentum/gen/pedestrian_path.h"

#include "konbini/city/pedestrian_path_contract.h"

// @implements spec/interface/figmentum-city-generation.md Pedestrian path contract

namespace konbini::adapters::figmentum {

// `planCity(params, plan.seed)` の plan から Figmentum の歩行者 network を導出し、
// game-owned な contract へ写す。Figmentum の error は code 名を残した
// std::runtime_error にして world load を止める。空 network や直線への
// fallback は行わない。
// @implements spec/interface/figmentum-city-generation.md Pedestrian path contract
[[nodiscard]] city::PedestrianPathContract projectPedestrianNetwork(
    const fg::CityPlanParams& params, const fg::CityPlan& plan);

// 既に導出済みの network を写す。version / seed が plan と食い違えば reject。
// @implements spec/interface/figmentum-city-generation.md Pedestrian path contract
[[nodiscard]] city::PedestrianPathContract convertPedestrianNetwork(
    const fg::PedestrianNetwork& network, const fg::CityPlan& plan);

}  // namespace konbini::adapters::figmentum
