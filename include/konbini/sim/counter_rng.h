#pragma once

#include <cstdint>

// @implements spec/design.md 5. Tick と決定性
// @implements spec/data/save-format.md 決定性

namespace konbini::sim {

// DEC-RNG-01 の RandomAlgorithmId。save へ書き出し、読み込み時の不一致は
// 互換 error とする (spec/data/save-format.md#決定性)。
inline constexpr std::uint64_t kRandomAlgorithmId = 0x53504C49544D4958ull;

enum class RandomStreamId : std::uint64_t {
    FirstPlayablePopulation = 0x46505F504F50554Cull,
};

struct RandomCounter {
    std::uint64_t worldSeed = 0;
    RandomStreamId stream = RandomStreamId::FirstPlayablePopulation;
    std::uint64_t tick = 0;
    std::uint64_t stableId = 0;
    std::uint64_t ordinal = 0;
};

[[nodiscard]] std::uint64_t splitMix64(std::uint64_t value) noexcept;
[[nodiscard]] std::uint64_t counterRandom(const RandomCounter& counter) noexcept;

}  // namespace konbini::sim
