#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace konbini::sim {
enum class SkillId : std::uint8_t {
    MultilingualStaff, Delivery, Coffee, Bakery, HotSnacks, QualityIngredients,
    ReusedShop, SecurityCamera, NightShift, RaisedBottom, MindWave, Count
};
inline constexpr std::size_t kSkillCount = static_cast<std::size_t>(SkillId::Count);
struct SkillState {
    std::array<std::uint32_t, kSkillCount> ranks{};
    std::array<SkillId, 3> offers{SkillId::Count, SkillId::Count, SkillId::Count};
    std::uint32_t level = 1, experience = 0;
    bool pending = false;
    std::uint64_t nextCoffeeTick = 0, nextSnacksTick = 0, nextWaveTick = 0;
    std::uint64_t snacksEndTick = 0, waveEndTick = 0, coffeeAtTick = 0;
    std::int64_t coffeePayout = 0;
};
inline std::uint32_t skillRank(const SkillState& state, SkillId id) noexcept {
    return state.ranks[static_cast<std::size_t>(id)];
}
struct SkillInfluence {
    // -1 means no player override; regular ownership rules apply.
    int playerChain = -1;
    std::uint32_t bonus = 0;
    bool mindWave = false;
};
} // namespace konbini::sim
