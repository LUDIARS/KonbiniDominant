#pragma once

#include <array>

#include "konbini/sim/chain_id.h"
#include "konbini/sim/facility_table.h"

namespace konbini::render {

using WorldColor = std::array<float, 4>;

[[nodiscard]] WorldColor chainColor(sim::ChainId chain, float alpha);
[[nodiscard]] WorldColor facilityColor(
    bool isBuildable, sim::FacilityState state);
[[nodiscard]] WorldColor selectedFacilityColor() noexcept;

}  // namespace konbini::render
