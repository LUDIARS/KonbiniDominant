#include "konbini/app/command_composer.h"

#include <limits>
#include <stdexcept>

// @implements spec/data/world-state.md Commands

namespace konbini::app {

sim::CommandOrder CommandComposer::nextOrder(const std::uint64_t targetTick) {
    if (sequence_ == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error("player command sequence exhausted");
    }
    return {
        .targetTick = targetTick,
        .sourcePriority = sim::CommandSourcePriority::Player,
        .sequence = sequence_++,
    };
}

// @implements spec/feature/chain-selection.md 共通
sim::PlayerCommand CommandComposer::selectChain(
    const sim::ChainId chain, const std::uint64_t targetTick) {
    if (!sim::isFirstPlayableChainId(chain)) {
        // 未知の chain を simulation の validation 任せにすると、入力層の
        // 取り違え (index 由来のずれ) が失敗 command として静かに流れる。
        throw std::invalid_argument(
            "select chain command requires a first-playable chain");
    }
    return sim::SelectChainCommand{
        .order = nextOrder(targetTick),
        .chain = chain,
    };
}

// @implements spec/feature/phase-1-dominant-triangle.md 店舗配置
sim::PlayerCommand CommandComposer::placeStore(
    const sim::ChainId chain, const sim::FacilityId facilityId,
    const std::uint64_t targetTick, const std::uint32_t verticalSlot) {
    if (!sim::isFirstPlayableChainId(chain)) {
        throw std::invalid_argument(
            "place store command requires a first-playable chain");
    }
    if (!facilityId.isValid()) {
        throw std::invalid_argument(
            "place store command requires a live facility id");
    }
    return sim::PlaceStoreCommand{
        .order = nextOrder(targetTick),
        .chain = chain,
        .facilityId = facilityId,
        .verticalSlot = verticalSlot,
    };
}

sim::PlayerCommand CommandComposer::campaignAction(const sim::ChainId chain, const sim::CampaignAction action,
    const sim::FacilityId facility, const std::uint32_t verticalSlot, const std::uint32_t dimension,
    const std::uint64_t targetTick) {
    if (!sim::isFirstPlayableChainId(chain)) throw std::invalid_argument("invalid player chain");
    return sim::CampaignCommand{nextOrder(targetTick),chain,action,facility,verticalSlot,dimension};
}
std::uint64_t CommandComposer::issuedCount() const noexcept {
    return sequence_;
}

}  // namespace konbini::app
