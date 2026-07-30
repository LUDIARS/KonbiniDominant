#include "konbini/sim/chain_economy_table.h"

#include <limits>
#include <stdexcept>

// @implements spec/data/world-state.md ChainEconomyTable
// @implements spec/feature/economy-and-population.md 初期資金
// @implements spec/feature/economy-and-population.md 建設費

namespace konbini::sim {

namespace {

// content / save 由来の未検証 ChainId を dense slot へ落とす唯一の入口。
// @implements spec/data/world-state.md ID
std::size_t checkedChainIndex(const ChainId chain) {
    if (!isFirstPlayableChainId(chain)) {
        throw std::invalid_argument("chain id is outside first-playable range");
    }
    return chainIndex(chain);
}

}  // namespace

// @implements spec/data/world-state.md ChainEconomyTable
ChainEconomyTable::ChainEconomyTable(const FirstPlayableContent& content)
    : rules_(content.chains),
      startingStoreEquivalent_(content.startingStoreEquivalent) {
    for (std::size_t index = 0; index < rules_.size(); ++index) {
        if (chainIndex(rules_[index].id) != index) {
            throw std::invalid_argument(
                "first-playable content chains are not indexed by chain id");
        }
    }
}

// @implements spec/data/world-state.md ChainEconomyTable
ChainEconomyRow ChainEconomyTable::row(const ChainId chain) const {
    const std::size_t index = checkedChainIndex(chain);
    return {
        .chain = chain,
        .cashCredits = cashCredits_[index],
        .storeCount = storeCounts_[index],
        .customerShare = customerShares_[index],
        .incomeThisTick = incomeThisTick_[index],
        .expenseThisTick = expenseThisTick_[index],
        .isActive = isActive_[index] != 0,
    };
}

// system 側が content を再度引かずに済むよう、economy が保持している正本の
// rule を返す。content と table が別 instance でも数値が食い違わない。
// @implements spec/data/content-schema.md Chain
const ChainContent& ChainEconomyTable::rules(const ChainId chain) const {
    return rules_[checkedChainIndex(chain)];
}

// REQ-ECON-01: 初期資金は base 建設費 × startingStoreEquivalent。倍率と単価は
// content 側の値なので、積が int64 を超える content は fail-fast させる。
// @implements spec/feature/economy-and-population.md 初期資金
bool ChainEconomyTable::activate(const ChainId chain) {
    const std::size_t index = checkedChainIndex(chain);
    if (isActive_[index] != 0) {
        return false;
    }
    if (startingStoreEquivalent_ == 0) {
        throw std::invalid_argument(
            "starting store equivalent must be positive");
    }
    const std::int64_t cost = rules_[index].buildCostCredits;
    if (cost <= 0 ||
        cost > std::numeric_limits<std::int64_t>::max() /
                   static_cast<std::int64_t>(startingStoreEquivalent_)) {
        throw std::overflow_error("starting cash overflow");
    }
    cashCredits_[index] =
        cost * static_cast<std::int64_t>(startingStoreEquivalent_);
    isActive_[index] = 1;
    return true;
}

// @implements spec/feature/economy-and-population.md 建設費
bool ChainEconomyTable::canAfford(const ChainId chain,
                                  const std::int64_t credits) const {
    const std::size_t index = checkedChainIndex(chain);
    return credits >= 0 && isActive_[index] != 0 &&
           cashCredits_[index] >= credits;
}

// placement 失敗時は費用を消費しない (economy-and-population.md#建設費)。
// 資金・expense・storeCount は同じ呼び出しで確定させ、部分適用を残さない。
// @implements spec/feature/economy-and-population.md 建設費
bool ChainEconomyTable::debitPlacement(const ChainId chain,
                                       const std::int64_t credits) {
    if (!canAfford(chain, credits)) {
        return false;
    }
    const std::size_t index = checkedChainIndex(chain);
    if (expenseThisTick_[index] >
            std::numeric_limits<std::int64_t>::max() - credits ||
        storeCounts_[index] == std::numeric_limits<std::uint32_t>::max()) {
        throw std::overflow_error("placement economy counter overflow");
    }
    cashCredits_[index] -= credits;
    expenseThisTick_[index] += credits;
    ++storeCounts_[index];
    return true;
}

// @implements spec/data/world-state.md ChainEconomyTable
void ChainEconomyTable::resetTickMetrics() noexcept {
    incomeThisTick_.fill(0);
    expenseThisTick_.fill(0);
}

// @implements spec/feature/economy-and-population.md 顧客化
void ChainEconomyTable::setCustomerShare(
    const ChainId chain, const std::uint64_t population) {
    customerShares_[checkedChainIndex(chain)] = population;
}

// @implements spec/feature/economy-and-population.md 収益
void ChainEconomyTable::creditRevenue(const ChainId chain,
                                      const std::int64_t credits) {
    if (credits < 0) {
        throw std::invalid_argument("revenue must be non-negative");
    }
    const std::size_t index = checkedChainIndex(chain);
    if (cashCredits_[index] > std::numeric_limits<std::int64_t>::max() - credits ||
        incomeThisTick_[index] >
            std::numeric_limits<std::int64_t>::max() - credits) {
        throw std::overflow_error("chain revenue overflow");
    }
    cashCredits_[index] += credits;
    incomeThisTick_[index] += credits;
}

}  // namespace konbini::sim
