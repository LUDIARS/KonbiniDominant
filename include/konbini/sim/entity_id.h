#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include "konbini/sim/dense_column.h"

// @implements spec/data/world-state.md ID

namespace konbini::sim {

struct EntityId {
    std::uint32_t index = 0;
    std::uint32_t generation = 0;

    auto operator<=>(const EntityId&) const = default;
};

// Tag ごとに別型になるので、`FacilityId` を要求する API へ `StoreId` を渡す
// 誤りは compile error になる。aggregate の直接初期化だけを許し、
// 生の index/generation 対が暗黙に entity 参照へ昇格しないようにする。
template <typename Tag>
struct TypedEntityId {
    EntityId rawValue{};

    // @implements spec/data/world-state.md ID
    [[nodiscard]] constexpr EntityId value() const noexcept { return rawValue; }

    // generation 0 は「未設定 handle」の予約値。default 構築した ID を
    // 実在 entity への参照として table へ入れない。
    // @implements spec/data/world-state.md ID
    [[nodiscard]] constexpr bool isValid() const noexcept {
        return rawValue.generation != 0;
    }

    auto operator<=>(const TypedEntityId&) const = default;
};

struct FacilityIdTag;
struct StoreIdTag;
struct PopulationCellIdTag;

using FacilityId = TypedEntityId<FacilityIdTag>;
using StoreId = TypedEntityId<StoreIdTag>;
using PopulationCellId = TypedEntityId<PopulationCellIdTag>;

// 解放済み index を再利用しつつ、再利用のたびに generation を進めることで
// 古い handle を stale と判定できるようにする pool。
// @implements spec/data/world-state.md ID
template <typename Id>
class GenerationalIdPool {
public:
    // free list を LIFO で消費する。空なら新しい index を 1 つ確保する。
    // @implements spec/data/world-state.md ID
    [[nodiscard]] Id acquire() {
        if (!freeIndices_.empty()) {
            const std::uint32_t index = freeIndices_.back();
            freeIndices_.pop_back();
            alive_[index] = 1U;
            return Id{EntityId{index, generations_[index]}};
        }

        if (generations_.size() >=
            static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
            throw std::overflow_error("generational entity index space exhausted");
        }

        const auto index = static_cast<std::uint32_t>(generations_.size());
        const std::size_t nextSize = generations_.size() + 1;
        reserveForAppend(generations_, nextSize);
        reserveForAppend(alive_, nextSize);
        generations_.push_back(1);
        alive_.push_back(1U);
        return Id{EntityId{index, 1}};
    }

    // 既に解放済み / stale な handle は false を返し、free list へ index を
    // 二重登録しない。二重登録すると同じ index が 2 回 acquire されて
    // 別 entity が同一 ID を共有する。
    // @implements spec/data/world-state.md ID
    [[nodiscard]] bool release(const Id id) {
        if (!isAlive(id)) {
            return false;
        }

        const std::uint32_t index = id.value().index;
        reserveForAppend(freeIndices_, freeIndices_.size() + 1);
        freeIndices_.push_back(index);
        alive_[index] = 0U;
        // generation 0 は「未設定 handle」の予約値なので、wrap した時だけ
        // 1 へ飛ばす。同一 index を 2^32 回再利用すると generation が一周し、
        // 同世代の古い handle と衝突しうる (ABA)。実 playtime では届かない。
        ++generations_[index];
        if (generations_[index] == 0) {
            generations_[index] = 1;
        }
        return true;
    }

    // @implements spec/data/world-state.md ID
    [[nodiscard]] bool isAlive(const Id id) const noexcept {
        const EntityId value = id.value();
        return value.generation != 0 && value.index < generations_.size() &&
               alive_[value.index] != 0 &&
               generations_[value.index] == value.generation;
    }

private:
    std::vector<std::uint32_t> generations_;
    std::vector<std::uint8_t> alive_;
    std::vector<std::uint32_t> freeIndices_;
};

}  // namespace konbini::sim
