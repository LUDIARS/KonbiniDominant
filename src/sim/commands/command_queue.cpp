#include "konbini/sim/command_queue.h"

#include <algorithm>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

// @implements spec/data/world-state.md Commands

namespace konbini::sim {

namespace {

std::uint8_t commandKind(const PlayerCommand& command) noexcept {
    return static_cast<std::uint8_t>(command.index());
}

auto facilityOrder(const FacilityId id) noexcept {
    return std::pair{id.value().index, id.value().generation};
}

bool payloadLess(const PlayerCommand& left, const PlayerCommand& right) noexcept {
    if (left.index() != right.index()) {
        return left.index() < right.index();
    }
    return std::visit(
        [](const auto& leftCommand, const auto& rightCommand) {
            using Left = std::decay_t<decltype(leftCommand)>;
            using Right = std::decay_t<decltype(rightCommand)>;
            if constexpr (!std::is_same_v<Left, Right>) {
                return false;
            } else if constexpr (std::is_same_v<Left, SelectChainCommand>) {
                return chainIndex(leftCommand.chain) <
                       chainIndex(rightCommand.chain);
            } else {
                return std::tuple{chainIndex(leftCommand.chain),
                                  facilityOrder(leftCommand.facilityId),
                                  leftCommand.verticalSlot} <
                       std::tuple{chainIndex(rightCommand.chain),
                                  facilityOrder(rightCommand.facilityId),
                                  rightCommand.verticalSlot};
            }
        },
        left,
        right);
}

}  // namespace

// @implements spec/data/world-state.md Commands
const CommandOrder& commandOrder(const PlayerCommand& command) noexcept {
    return std::visit(
        [](const auto& value) -> const CommandOrder& { return value.order; },
        command);
}

// 同 tick 内は `player/AI priority → sequence` で安定 sort する
// (world-state.md#Commands)。sequence まで一致した場合も payload まで見て
// 全順序にし、queue への push 順で結果が変わらないようにする。
// @implements spec/data/world-state.md Commands
bool commandLess(const PlayerCommand& left, const PlayerCommand& right) noexcept {
    const CommandOrder& leftOrder = commandOrder(left);
    const CommandOrder& rightOrder = commandOrder(right);
    const auto leftKey =
        std::tuple{leftOrder.targetTick,
                   static_cast<std::uint8_t>(leftOrder.sourcePriority),
                   leftOrder.sequence,
                   commandKind(left)};
    const auto rightKey =
        std::tuple{rightOrder.targetTick,
                   static_cast<std::uint8_t>(rightOrder.sourcePriority),
                   rightOrder.sequence,
                   commandKind(right)};
    if (leftKey != rightKey) {
        return leftKey < rightKey;
    }
    return payloadLess(left, right);
}

// invalid command は world state を部分変更しないので、chain 範囲外と
// (tick, source, sequence) 重複はここで弾く。重複を許すと同 tick の
// 適用順が push 順に依存し、決定性が崩れる。
// @implements spec/data/world-state.md Commands
void CommandQueue::push(PlayerCommand command) {
    const bool validChain = std::visit(
        [](const auto& value) { return isFirstPlayableChainId(value.chain); },
        command);
    if (!validChain) {
        throw std::invalid_argument(
            "player command contains an invalid first-playable chain");
    }
    const CommandOrder& newOrder = commandOrder(command);
    for (const PlayerCommand& pending : pending_) {
        const CommandOrder& pendingOrder = commandOrder(pending);
        if (pendingOrder.targetTick == newOrder.targetTick &&
            pendingOrder.sourcePriority == newOrder.sourcePriority &&
            pendingOrder.sequence == newOrder.sequence) {
            throw std::invalid_argument(
                "command sequence must be unique per tick and source");
        }
    }
    pending_.push_back(std::move(command));
}

// 過ぎた tick を狙う command は無言で捨てず、除去したうえで throw する。
// 黙って適用すると tick 境界がずれ、再生時に state hash が一致しなくなる。
// @implements spec/data/world-state.md Commands
// @implements spec/design.md 5. Tick と決定性
std::vector<PlayerCommand> CommandQueue::takeForTick(const std::uint64_t tick) {
    std::sort(pending_.begin(), pending_.end(), commandLess);
    auto firstCurrent = pending_.begin();
    while (firstCurrent != pending_.end() &&
           commandOrder(*firstCurrent).targetTick < tick) {
        ++firstCurrent;
    }
    if (firstCurrent != pending_.begin()) {
        pending_.erase(pending_.begin(), firstCurrent);
        throw std::logic_error("stale player command remained in the queue");
    }

    std::vector<PlayerCommand> ready;
    auto firstFuture = pending_.begin();
    while (firstFuture != pending_.end() &&
           commandOrder(*firstFuture).targetTick == tick) {
        ready.push_back(std::move(*firstFuture));
        ++firstFuture;
    }
    pending_.erase(pending_.begin(), firstFuture);
    return ready;
}

std::size_t CommandQueue::pendingCount() const noexcept {
    return pending_.size();
}

}  // namespace konbini::sim
