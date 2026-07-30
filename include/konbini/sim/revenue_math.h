#pragma once

#include <cstdint>

namespace konbini::sim {

// @implements spec/feature/economy-and-population.md 収益
[[nodiscard]] std::int64_t calculateRevenueCredits(
    std::uint64_t population, std::int64_t milliCreditsPerPerson);

}  // namespace konbini::sim
