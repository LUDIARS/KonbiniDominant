#pragma once

#include <array>
#include <cstdint>

#include "konbini/sim/chain_id.h"
#include "konbini/sim/first_playable_content.h"

// @implements spec/data/world-state.md ChainEconomyTable

namespace konbini::sim {

// 読み出し用の値 row。table 内部は chain ごとの flat array (SoA) で持ち、
// row struct を storage にはしない。
// @implements spec/data/world-state.md ChainEconomyTable
struct ChainEconomyRow {
    ChainId chain = ChainId::Losan;
    std::int64_t cashCredits = 0;
    std::uint32_t storeCount = 0;
    std::uint64_t customerShare = 0;
    std::int64_t incomeThisTick = 0;
    std::int64_t expenseThisTick = 0;
    bool isActive = false;
};

// @implements spec/data/world-state.md ChainEconomyTable
// @implements spec/feature/economy-and-population.md 初期資金
class ChainEconomyTable {
public:
    explicit ChainEconomyTable(const FirstPlayableContent& content);

    [[nodiscard]] ChainEconomyRow row(ChainId chain) const;
    [[nodiscard]] const ChainContent& rules(ChainId chain) const;

    [[nodiscard]] bool activate(ChainId chain);
    [[nodiscard]] bool canAfford(ChainId chain, std::int64_t credits) const;
    [[nodiscard]] bool debitPlacement(ChainId chain, std::int64_t credits);
    void resetTickMetrics() noexcept;
    void setCustomerShare(ChainId chain, std::uint64_t population);
    void creditRevenue(ChainId chain, std::int64_t credits);

private:
    std::array<ChainContent, kFirstPlayableChainCount> rules_;
    std::array<std::int64_t, kFirstPlayableChainCount> cashCredits_{};
    std::array<std::uint32_t, kFirstPlayableChainCount> storeCounts_{};
    std::array<std::uint64_t, kFirstPlayableChainCount> customerShares_{};
    std::array<std::int64_t, kFirstPlayableChainCount> incomeThisTick_{};
    std::array<std::int64_t, kFirstPlayableChainCount> expenseThisTick_{};
    std::array<std::uint8_t, kFirstPlayableChainCount> isActive_{};
    std::uint32_t startingStoreEquivalent_ = 0;
};

}  // namespace konbini::sim
