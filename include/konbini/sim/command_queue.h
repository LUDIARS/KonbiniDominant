#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "konbini/sim/player_command.h"

// @implements spec/data/world-state.md Commands

namespace konbini::sim {

class CommandQueue {
public:
    void push(PlayerCommand command);
    [[nodiscard]] std::vector<PlayerCommand> takeForTick(std::uint64_t tick);
    [[nodiscard]] std::size_t pendingCount() const noexcept;

private:
    std::vector<PlayerCommand> pending_;
};

}  // namespace konbini::sim
