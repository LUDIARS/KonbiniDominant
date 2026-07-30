#pragma once

#include <compare>
#include <cstdint>
#include <variant>

#include "konbini/sim/chain_id.h"
#include "konbini/sim/entity_id.h"

// @implements spec/data/world-state.md Commands

namespace konbini::sim {

enum class CommandSourcePriority : std::uint8_t {
    Player = 0,
    Ai = 1,
};

struct CommandOrder {
    std::uint64_t targetTick = 0;
    CommandSourcePriority sourcePriority = CommandSourcePriority::Player;
    std::uint64_t sequence = 0;

    auto operator<=>(const CommandOrder&) const = default;
};

struct SelectChainCommand {
    CommandOrder order{};
    ChainId chain = ChainId::Losan;
};

struct PlaceStoreCommand {
    CommandOrder order{};
    ChainId chain = ChainId::Losan;
    FacilityId facilityId{};
    std::uint32_t verticalSlot = 0;
};

using PlayerCommand = std::variant<SelectChainCommand, PlaceStoreCommand>;

[[nodiscard]] const CommandOrder& commandOrder(const PlayerCommand& command) noexcept;
[[nodiscard]] bool commandLess(const PlayerCommand& left,
                               const PlayerCommand& right) noexcept;

}  // namespace konbini::sim
