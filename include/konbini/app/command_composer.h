#pragma once

#include <cstdint>

#include "konbini/sim/player_command.h"

// @implements spec/data/world-state.md Commands
// @implements spec/interface/ergo-runtime.md Frame / simulation

namespace konbini::app {

// 入力由来の command へ決定的な順序を与える発行体。
//
// `CommandOrder` は (targetTick, sourcePriority, sequence) で全順序になる。
// sequence を frame ごとに 0 から振り直すと、同 tick 内の 2 command が同じ
// 順序値を持ちうるため、session を通して単調増加させる。
class CommandComposer {
public:
    [[nodiscard]] sim::PlayerCommand selectChain(
        sim::ChainId chain, std::uint64_t targetTick);

    [[nodiscard]] sim::PlayerCommand placeStore(
        sim::ChainId chain, sim::FacilityId facilityId,
        std::uint64_t targetTick, std::uint32_t verticalSlot = 0);

    [[nodiscard]] sim::PlayerCommand campaignAction(sim::ChainId chain, sim::CampaignAction action,
        sim::FacilityId facility, std::uint32_t verticalSlot, std::uint32_t dimension, std::uint64_t targetTick);

    [[nodiscard]] std::uint64_t issuedCount() const noexcept;

private:
    [[nodiscard]] sim::CommandOrder nextOrder(std::uint64_t targetTick);

    std::uint64_t sequence_ = 0;
};

}  // namespace konbini::app
