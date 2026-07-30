#include "konbini/sim/counter_rng.h"

// @implements spec/design.md 5. Tick と決定性
// @implements spec/data/save-format.md 決定性

namespace konbini::sim {

namespace {

// 各 counter field を別 domain で pre-mix し、tick / stableId / ordinal の
// 単純な入れ替えが同じ値にならないようにする。値は save 互換の一部であり、
// 変更する場合は kRandomAlgorithmId も上げる。
constexpr std::uint64_t kTickDomain = 0x5449434B5F563031ull;
constexpr std::uint64_t kStableIdDomain = 0x535441424C455F49ull;
constexpr std::uint64_t kOrdinalDomain = 0x4F5244494E414C31ull;

}  // namespace

// DEC-RNG-01 の counterless hash 基底。published splitmix64 と bit 一致させる
// (reference vector は test で pin 済み)。
// @implements spec/design.md 5. Tick と決定性
std::uint64_t splitMix64(std::uint64_t value) noexcept {
    value += 0x9E3779B97F4A7C15ull;
    value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ull;
    value = (value ^ (value >> 27U)) * 0x94D049BB133111EBull;
    return value ^ (value >> 31U);
}

// DEC-RNG-01 の
// `(worldSeed, randomAlgorithm, streamId, tick, stableId, ordinal)` を
// 消費位置なしで 1 draw へ写す。
// @implements spec/data/save-format.md 決定性
std::uint64_t counterRandom(const RandomCounter& counter) noexcept {
    std::uint64_t value = splitMix64(counter.worldSeed ^ kRandomAlgorithmId);
    value = splitMix64(value ^ static_cast<std::uint64_t>(counter.stream));
    value = splitMix64(value ^ splitMix64(counter.tick ^ kTickDomain));
    value = splitMix64(value ^ splitMix64(counter.stableId ^ kStableIdDomain));
    return splitMix64(value ^ splitMix64(counter.ordinal ^ kOrdinalDomain));
}

}  // namespace konbini::sim
