#include "konbini/sim/revenue_math.h"

#include <limits>
#include <stdexcept>

// @implements spec/feature/economy-and-population.md 収益

namespace konbini::sim {

// population は千人単位ではなく人単位、rate は milli-credit / 人なので、
// 積を 1000 で割る前に 64bit を溢れさせないよう商と剰余へ分解する。HUD の
// 予測と tick 決算が同じ値を出すよう、計算はこの関数だけに置く。
// @implements spec/feature/economy-and-population.md 収益
std::int64_t calculateRevenueCredits(
    const std::uint64_t population,
    const std::int64_t milliCreditsPerPerson) {
    if (milliCreditsPerPerson < 0) {
        throw std::invalid_argument("revenue rate must be non-negative");
    }

    constexpr std::uint64_t kMilliScale = 1000;
    constexpr std::uint64_t kMaxCredits =
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    const std::uint64_t rate =
        static_cast<std::uint64_t>(milliCreditsPerPerson);
    const std::uint64_t wholePopulation = population / kMilliScale;
    const std::uint64_t remainderPopulation = population % kMilliScale;

    if (rate != 0 && wholePopulation > kMaxCredits / rate) {
        throw std::overflow_error("revenue exceeds signed credit range");
    }
    const std::uint64_t wholeRevenue = wholePopulation * rate;

    // Both remainders are below 1000, so this computes
    // floor(remainderPopulation * rate / 1000) without overflowing first.
    const std::uint64_t rateWhole = rate / kMilliScale;
    const std::uint64_t rateRemainder = rate % kMilliScale;
    const std::uint64_t fractionalRevenue =
        remainderPopulation * rateWhole +
        (remainderPopulation * rateRemainder) / kMilliScale;
    if (wholeRevenue > kMaxCredits - fractionalRevenue) {
        throw std::overflow_error("revenue exceeds signed credit range");
    }
    return static_cast<std::int64_t>(wholeRevenue + fractionalRevenue);
}

std::int64_t calculateStoreRevenueCredits(
    const std::uint64_t population, const std::int64_t milliCreditsPerPerson,
    const std::uint32_t revenuePermille) {
    if (revenuePermille < 1000 || revenuePermille > 10000) {
        throw std::invalid_argument("invalid store revenue multiplier");
    }
    const auto base = calculateRevenueCredits(population, milliCreditsPerPerson);
    return calculateRevenueCredits(static_cast<std::uint64_t>(base), revenuePermille);
}
}  // namespace konbini::sim
