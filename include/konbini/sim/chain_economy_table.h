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
    [[nodiscard]] bool activateWithCash(ChainId chain, std::int64_t initialCashCredits);
    [[nodiscard]] bool canAfford(ChainId chain, std::int64_t credits) const;
    [[nodiscard]] bool debitPlacement(ChainId chain, std::int64_t credits);
    void resetTickMetrics() noexcept;
    void setCustomerShare(ChainId chain, std::uint64_t population);
    void creditRevenue(ChainId chain, std::int64_t credits);
    void removeStore(ChainId chain);
    [[nodiscard]] bool spend(ChainId chain, std::int64_t credits);
    [[nodiscard]] const std::optional<CampaignContent>& campaignRules() const noexcept { return campaign_; }

private:
    std::array<ChainContent, kSimulationChainCount> rules_;
    std::array<std::int64_t, kSimulationChainCount> cashCredits_{};
    std::array<std::uint32_t, kSimulationChainCount> storeCounts_{};
    std::array<std::uint64_t, kSimulationChainCount> customerShares_{};
    std::array<std::int64_t, kSimulationChainCount> incomeThisTick_{};
    std::array<std::int64_t, kSimulationChainCount> expenseThisTick_{};
    std::array<std::uint8_t, kSimulationChainCount> isActive_{};
    std::uint32_t startingStoreEquivalent_ = 0;
    std::optional<CampaignContent> campaign_;
};

}  // namespace konbini::sim
