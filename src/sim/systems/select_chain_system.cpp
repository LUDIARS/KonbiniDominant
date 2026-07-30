#include "konbini/sim/select_chain_system.h"

// @implements spec/feature/chain-selection.md 共通
// @implements spec/feature/game-flow.md 状態機械

namespace konbini::sim {

// ChainSelect → Phase1 の遷移と初期資金付与を同じ呼び出しで確定させる。
// 失敗時は playerChain と phase の片方だけを進めない。
// @implements spec/feature/chain-selection.md 共通
// @implements spec/feature/game-flow.md 状態機械
SelectChainFailure applySelectChain(const SelectChainCommand& command,
                                    GameState& state,
                                    ChainEconomyTable& economy) {
    if (!isFirstPlayableChainId(command.chain)) {
        return SelectChainFailure::InvalidChain;
    }
    if (state.phase != GamePhase::ChainSelect) {
        return SelectChainFailure::WrongPhase;
    }
    if (state.playerChain.has_value()) {
        return SelectChainFailure::ChainAlreadySelected;
    }
    if (!economy.activate(command.chain)) {
        return SelectChainFailure::ChainAlreadyActive;
    }
    state.playerChain = command.chain;
    state.phase = GamePhase::Phase1;
    return SelectChainFailure::None;
}

}  // namespace konbini::sim
