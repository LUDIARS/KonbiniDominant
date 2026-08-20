#include "konbini/sim/chain_id.h"
#include "konbini/sim/counter_rng.h"
#include "konbini/sim/figmentum_facility_key.h"
#include "konbini/sim/math_types.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

// @implements spec/test/verification-strategy.md 1. Data / ID unit tests
// @implements spec/test/verification-strategy.md 2. Deterministic simulation

namespace {

int g_failures = 0;

void check(const bool ok, const char* const expression, const int line) {
    if (ok) {
        return;
    }
    ++g_failures;
    std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, line, expression);
}

#define CHECK(expression) check((expression), #expression, __LINE__)

using konbini::sim::Bounds3;
using konbini::sim::ChainId;
using konbini::sim::FigmentumFacilityKey;
using konbini::sim::RandomCounter;
using konbini::sim::RandomStreamId;
using konbini::sim::Vec3;
using konbini::sim::chainIdFromSlug;
using konbini::sim::chainIndex;
using konbini::sim::chainSlug;
using konbini::sim::counterRandom;
using konbini::sim::isFinite;
using konbini::sim::isFiniteAndOrdered;
using konbini::sim::isFirstPlayableChainId;
using konbini::sim::kFirstPlayableChainCount;
using konbini::sim::kRandomAlgorithmId;
using konbini::sim::splitMix64;

// --- ChainId -----------------------------------------------------------
// Enum values are persisted in saves and slugs are the content-data key, so
// both the numbering and the slug spelling are pinned here.

static_assert(chainIndex(ChainId::Losan) == 0);
static_assert(chainIndex(ChainId::Famoma) == 1);
static_assert(chainIndex(ChainId::SebanIleban) == 2);

static_assert(chainSlug(ChainId::Losan) == "losan");
static_assert(chainSlug(ChainId::Famoma) == "famoma");
static_assert(chainSlug(ChainId::SebanIleban) == "seban_ileban");

static_assert(chainIdFromSlug(chainSlug(ChainId::Losan)) == ChainId::Losan);
static_assert(chainIdFromSlug(chainSlug(ChainId::Famoma)) == ChainId::Famoma);
static_assert(chainIdFromSlug(chainSlug(ChainId::SebanIleban)) ==
              ChainId::SebanIleban);

static_assert(!chainIdFromSlug("").has_value());
static_assert(!chainIdFromSlug("LOSAN").has_value());
static_assert(!chainIdFromSlug("losan ").has_value());

static_assert(isFirstPlayableChainId(ChainId::Losan));
static_assert(isFirstPlayableChainId(ChainId::SebanIleban));
static_assert(!isFirstPlayableChainId(
    static_cast<ChainId>(static_cast<std::uint8_t>(kFirstPlayableChainCount))));
static_assert(!isFirstPlayableChainId(static_cast<ChainId>(200)));

// An unknown chain must degrade to an empty slug, never to another chain.
static_assert(chainSlug(static_cast<ChainId>(200)).empty());
static_assert(!chainIdFromSlug(chainSlug(static_cast<ChainId>(200))).has_value());

// --- FigmentumFacilityKey ----------------------------------------------

static_assert(!FigmentumFacilityKey{}.isValid());
static_assert(FigmentumFacilityKey{}.value() == 0);
static_assert(FigmentumFacilityKey{7}.isValid());
static_assert(FigmentumFacilityKey{7}.value() == 7);
static_assert(FigmentumFacilityKey{7} == FigmentumFacilityKey{7});
static_assert(FigmentumFacilityKey{1} != FigmentumFacilityKey{2});
static_assert(FigmentumFacilityKey{1} < FigmentumFacilityKey{2});

void testBoundsValidation() {
    constexpr double kInfinity = std::numeric_limits<double>::infinity();
    const double quietNan = std::numeric_limits<double>::quiet_NaN();

    CHECK(isFinite(Vec3{1.0, -2.0, 3.0}));
    CHECK(!isFinite(Vec3{kInfinity, 0.0, 0.0}));
    CHECK(!isFinite(Vec3{0.0, -kInfinity, 0.0}));
    CHECK(!isFinite(Vec3{0.0, 0.0, quietNan}));

    CHECK(isFiniteAndOrdered(Bounds3{Vec3{-1.0, -1.0, -1.0}, Vec3{1.0, 1.0, 1.0}}));

    // Degenerate extents are rejected on every axis.
    CHECK(!isFiniteAndOrdered(Bounds3{Vec3{1.0, -1.0, -1.0}, Vec3{1.0, 1.0, 1.0}}));
    CHECK(!isFiniteAndOrdered(Bounds3{Vec3{-1.0, 1.0, -1.0}, Vec3{1.0, 1.0, 1.0}}));
    CHECK(!isFiniteAndOrdered(Bounds3{Vec3{-1.0, -1.0, 1.0}, Vec3{1.0, 1.0, 1.0}}));

    // Inverted and non-finite bounds are rejected instead of silently accepted.
    CHECK(!isFiniteAndOrdered(Bounds3{Vec3{2.0, -1.0, -1.0}, Vec3{1.0, 1.0, 1.0}}));
    CHECK(!isFiniteAndOrdered(
        Bounds3{Vec3{0.0, 0.0, 0.0}, Vec3{kInfinity, 1.0, 1.0}}));
    CHECK(!isFiniteAndOrdered(
        Bounds3{Vec3{quietNan, 0.0, 0.0}, Vec3{1.0, 1.0, 1.0}}));
    CHECK(!isFiniteAndOrdered(
        Bounds3{Vec3{0.0, 0.0, 0.0}, Vec3{1.0, quietNan, 1.0}}));
}

RandomCounter baseCounter() {
    RandomCounter counter;
    counter.worldSeed = 0x0123456789ABCDEFull;
    counter.stream = RandomStreamId::FirstPlayablePopulation;
    counter.tick = 42;
    counter.stableId = 7;
    counter.ordinal = 0;
    return counter;
}

void testRandomAlgorithmIsPinned() {
    // DEC-RNG-01 makes a RandomAlgorithmId mismatch a save-compatibility error,
    // so both the id and the hash it names have to be stable across releases.
    CHECK(kRandomAlgorithmId == 0x53504C49544D4958ull);

    // Reference vector of the published splitmix64 (first output for seed 0).
    CHECK(splitMix64(0) == 0xE220A8397B1DCDAFull);
}

void testCounterRandomIsCounterless() {
    const RandomCounter counter = baseCounter();
    const std::uint64_t first = counterRandom(counter);

    // No consumption position: repeating the same counter repeats the value,
    // and interleaving other draws does not shift it.
    CHECK(counterRandom(counter) == first);

    RandomCounter other = counter;
    other.ordinal = 1;
    static_cast<void>(counterRandom(other));
    CHECK(counterRandom(counter) == first);

    // Every counter field must actually reach the output.
    RandomCounter varied = counter;
    varied.worldSeed ^= 1ull;
    CHECK(counterRandom(varied) != first);

    varied = counter;
    varied.stream = static_cast<RandomStreamId>(
        static_cast<std::uint64_t>(counter.stream) + 1ull);
    CHECK(counterRandom(varied) != first);

    varied = counter;
    varied.tick += 1;
    CHECK(counterRandom(varied) != first);

    varied = counter;
    varied.stableId += 1;
    CHECK(counterRandom(varied) != first);

    varied = counter;
    varied.ordinal += 1;
    CHECK(counterRandom(varied) != first);
}

void testCounterRandomSeparatesFieldDomains() {
    // The per-field domain constants exist so that permuting values between
    // tick / stableId / ordinal does not land on the same draw. Without them a
    // store advancing a tick would replay another store's value.
    RandomCounter counter = baseCounter();
    counter.tick = 1;
    counter.stableId = 2;
    counter.ordinal = 3;
    const std::uint64_t reference = counterRandom(counter);

    RandomCounter permuted = counter;
    permuted.tick = 2;
    permuted.stableId = 1;
    CHECK(counterRandom(permuted) != reference);

    permuted = counter;
    permuted.tick = 3;
    permuted.ordinal = 1;
    CHECK(counterRandom(permuted) != reference);

    permuted = counter;
    permuted.stableId = 3;
    permuted.ordinal = 2;
    CHECK(counterRandom(permuted) != reference);

    // Shifting the whole counter by one field position must not alias either.
    permuted = counter;
    permuted.tick = 2;
    permuted.stableId = 3;
    permuted.ordinal = 1;
    CHECK(counterRandom(permuted) != reference);
}

void testCounterRandomSeparatesNeighbours() {
    // Neighbouring (stableId, ordinal) pairs are the common draw pattern for a
    // tick; they must not collide or repeat a row.
    constexpr std::size_t kIds = 8;
    constexpr std::size_t kOrdinals = 8;
    std::array<std::uint64_t, kIds * kOrdinals> values{};

    RandomCounter counter = baseCounter();
    for (std::size_t id = 0; id < kIds; ++id) {
        for (std::size_t ordinal = 0; ordinal < kOrdinals; ++ordinal) {
            counter.stableId = id;
            counter.ordinal = ordinal;
            values[(id * kOrdinals) + ordinal] = counterRandom(counter);
        }
    }

    for (std::size_t i = 0; i < values.size(); ++i) {
        for (std::size_t j = i + 1; j < values.size(); ++j) {
            if (values[i] == values[j]) {
                std::fprintf(stderr,
                             "FAIL counterRandom collision at %zu/%zu (%llx)\n",
                             i, j,
                             static_cast<unsigned long long>(values[i]));
                ++g_failures;
            }
        }
    }
}

}  // namespace

// @implements spec/test/verification-strategy.md 1. Data / ID unit tests
// @spec 1. Data / ID unit tests
int main() {
    testBoundsValidation();
    testRandomAlgorithmIsPinned();
    testCounterRandomIsCounterless();
    testCounterRandomSeparatesFieldDomains();
    testCounterRandomSeparatesNeighbours();

    if (g_failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failures);
        return EXIT_FAILURE;
    }
    std::fprintf(stdout, "all checks passed\n");
    return EXIT_SUCCESS;
}
