#include "konbini/adapters/ergo/layer_initialization_scope.h"

#include <algorithm>
#include <stdexcept>

// @implements spec/interface/ergo-runtime.md Render host

namespace konbini::adapters::ergo {

LayerInitializationScope::~LayerInitializationScope() {
    rollback();
}

void LayerInitializationScope::onInitialized(
    ::ergo::render::IRenderLayer& layer) {
    const auto existing =
        std::find(initialized_.begin(), initialized_.end(), &layer);
    if (existing != initialized_.end()) {
        // 同じ layer を 2 回 initialize したという事実自体が bug。黙って
        // 上書きすると rollback の順序が壊れる。
        throw std::logic_error(
            "render layer was initialized twice in one scope");
    }
    initialized_.push_back(&layer);
}

void LayerInitializationScope::onShutdown(
    ::ergo::render::IRenderLayer& layer) noexcept {
    const auto existing =
        std::find(initialized_.begin(), initialized_.end(), &layer);
    if (existing != initialized_.end()) {
        initialized_.erase(existing);
    }
}

void LayerInitializationScope::rollback() noexcept {
    while (!initialized_.empty()) {
        ::ergo::render::IRenderLayer* const layer = initialized_.back();
        initialized_.pop_back();
        if (layer == nullptr) {
            continue;
        }
        try {
            layer->shutdown();
        } catch (...) {
            // 1 つの解放失敗で残りの layer を放置しない。原因は既に
            // 投げられている初期化例外側が持っている。
        }
    }
}

void LayerInitializationScope::release() noexcept {
    initialized_.clear();
}

std::size_t LayerInitializationScope::initializedCount() const noexcept {
    return initialized_.size();
}

}  // namespace konbini::adapters::ergo
