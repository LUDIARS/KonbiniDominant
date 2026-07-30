#pragma once

#include <cstdint>

#include "konbini/sim/chain_economy_table.h"
#include "konbini/sim/store_table.h"

// @implements spec/feature/economy-and-population.md 収益

namespace konbini::sim {

[[nodiscard]] bool collectEconomyForCompletedTick(
    std::uint64_t completedTicks, std::uint32_t economyPeriodTicks,
    const StoreTable& stores, ChainEconomyTable& economy);

}  // namespace konbini::sim
