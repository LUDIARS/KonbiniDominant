#include "konbini/sim/economy_system.h"

#include <stdexcept>
#include <utility>

#include "konbini/sim/revenue_math.h"

// @implements spec/feature/economy-and-population.md 収益

namespace konbini::sim {

// 決算 tick かどうかを判定し、該当 tick でだけ chain 収益を確定させる。
// 途中の store で throw しても authoritative table は部分更新しない。
// @implements spec/feature/economy-and-population.md 収益
bool collectEconomyForCompletedTick(const std::uint64_t completedTicks,
                                    const std::uint32_t economyPeriodTicks,
                                    const StoreTable& stores,
                                    ChainEconomyTable& economy) {
    if (economyPeriodTicks == 0) {
        throw std::invalid_argument("economy period must be positive");
    }
    if (completedTicks == 0 ||
        completedTicks % economyPeriodTicks != 0) {
        return false;
    }

    ChainEconomyTable stagedEconomy = economy;
    for (std::size_t index = 0; index < stores.size(); ++index) {
        const StoreRow store = stores.row(index);
        if (!store.isActive) {
            continue;
        }
        const ChainContent& rules = stagedEconomy.rules(store.chain);
        stagedEconomy.creditRevenue(
            store.chain,
            calculateRevenueCredits(store.capturedPopulation,
                                    rules.revenueMilliCreditsPerPerson));
    }
    economy = std::move(stagedEconomy);
    return true;
}

}  // namespace konbini::sim
