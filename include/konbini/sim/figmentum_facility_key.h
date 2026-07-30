#pragma once

#include <compare>
#include <cstdint>

// @implements spec/interface/figmentum-city-generation.md Required game-side boundary

namespace konbini::sim {

// Figmentum の CityPlan 側 stable key。0 は「key 無し」を表す予約値で、
// SDF term index や mesh vertex index から作らない。全 uint64 値が表現可能
// なので、constructor を持たない trivial aggregate として dense column へ置く。
struct FigmentumFacilityKey {
    std::uint64_t rawValue = 0;

    // @implements spec/interface/figmentum-city-generation.md Required game-side boundary
    [[nodiscard]] constexpr std::uint64_t value() const noexcept {
        return rawValue;
    }

    // 予約値 0 を弾く。`plan_city` の stable ID collision / 未設定 key を
    // placeholder として続行させない。
    // @implements spec/interface/figmentum-city-generation.md Error contract
    [[nodiscard]] constexpr bool isValid() const noexcept {
        return rawValue != 0;
    }

    [[nodiscard]] auto operator<=>(const FigmentumFacilityKey&) const = default;
};

}  // namespace konbini::sim
