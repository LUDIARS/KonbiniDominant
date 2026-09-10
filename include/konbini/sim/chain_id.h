#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

// @implements spec/feature/chain-selection.md 共通
// @implements spec/data/save-format.md Store entry

namespace konbini::sim {

// save と content data の key になるため、既存値を renumber しない。
// spec/data/world-state.md の `ChainId` は boss 勢力も含むが、boss は
// first playable の範囲外なので、追加時は 3 以降を割り当てる。
enum class ChainId : std::uint8_t {
    Losan = 0,
    Famoma = 1,
    SebanIleban = 2,
    Aion = 3,
};

// New Game で選択可能な chain 数 (先頭 3 値)。boss 等を enum へ追加しても
// この値は増やさない。
inline constexpr std::size_t kFirstPlayableChainCount = 3;
inline constexpr std::size_t kSimulationChainCount = 4;
[[nodiscard]] constexpr bool isSimulationChainId(ChainId id) noexcept {
    return static_cast<std::size_t>(id) < kSimulationChainCount;
}

// @implements spec/feature/chain-selection.md 共通
[[nodiscard]] constexpr bool isFirstPlayableChainId(const ChainId id) noexcept {
    // `std::size_t` で比較する。`kFirstPlayableChainCount` を uint8 へ狭めると、
    // 将来 256 以上になった場合に wrap して全 id を playable と判定してしまう。
    return static_cast<std::size_t>(id) < kFirstPlayableChainCount;
}

// chain 単位の flat array を index する用途。save / content data 由来の値は
// 添字にする前に `isFirstPlayableChainId` を通す。未検証の値をそのまま渡すと
// 範囲外 index になる。
// @implements spec/data/world-state.md ID
[[nodiscard]] constexpr std::size_t chainIndex(const ChainId id) noexcept {
    return static_cast<std::size_t>(id);
}

// slug は save / content data の key 用であり、chain 差の実装分岐ではない
// (chain-selection.md: system 内の chain 名 switch を増やさない)。
// 未知値は空 slug へ落とし、他 chain へ誤解決しない。
// @implements spec/data/save-format.md Store entry
[[nodiscard]] constexpr std::string_view chainSlug(const ChainId id) noexcept {
    switch (id) {
        case ChainId::Losan:
            return "losan";
        case ChainId::Famoma:
            return "famoma";
        case ChainId::SebanIleban:
            return "seban_ileban";
        case ChainId::Aion: return "aion";
    }
    return {};
}

// save / content data 由来の未検証 slug を受ける入口。未知 slug は
// `std::nullopt` にし、無言 default の chain へ落とさない
// (save-format.md#Migration: field 欠落を無言 default で補わない)。
// @implements spec/data/save-format.md Store entry
[[nodiscard]] constexpr std::optional<ChainId> chainIdFromSlug(
    const std::string_view slug) noexcept {
    if (slug == "losan") {
        return ChainId::Losan;
    }
    if (slug == "famoma") {
        return ChainId::Famoma;
    }
    if (slug == "seban_ileban") {
        return ChainId::SebanIleban;
    }
    return std::nullopt;
}

}  // namespace konbini::sim
