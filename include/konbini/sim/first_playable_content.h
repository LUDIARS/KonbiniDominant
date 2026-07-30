#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

#include "konbini/sim/chain_id.h"

// @implements spec/data/content-schema.md Root
// @implements spec/data/content-schema.md Chain

namespace konbini::sim {

// @implements spec/data/content-schema.md Chain
struct ChainContent {
    ChainId id = ChainId::Losan;
    std::string displayName;
    std::int64_t buildCostCredits = 0;
    double zocRadiusMeters = 0.0;
    std::int64_t revenueMilliCreditsPerPerson = 0;
};

// @implements spec/data/content-schema.md Simulation
struct SimulationContent {
    std::uint32_t ticksPerSecond = 0;
    std::uint32_t economyPeriodTicks = 0;
    std::string randomAlgorithm;
};

struct PopulationContent {
    std::uint32_t basePopulation = 0;
    std::uint32_t randomPopulationCount = 0;
    std::string randomStream;
};

// @implements spec/data/content-schema.md Root
struct FirstPlayableContent {
    std::uint32_t schemaVersion = 0;
    std::uint32_t contentVersion = 0;
    SimulationContent simulation;
    PopulationContent population;
    std::uint32_t startingStoreEquivalent = 0;
    std::array<ChainContent, kFirstPlayableChainCount> chains{};

    [[nodiscard]] const ChainContent& chain(ChainId id) const;
};

// `chains` is indexed by ChainId so that every table can use chainIndex() as a
// dense slot. Content that violates that invariant must fail fast instead of
// silently charging another chain's build cost.
// @implements spec/data/content-schema.md Validation
inline const ChainContent& FirstPlayableContent::chain(const ChainId id) const {
    if (!isFirstPlayableChainId(id)) {
        throw std::invalid_argument("chain id is outside first-playable range");
    }
    const ChainContent& content = chains[chainIndex(id)];
    if (content.id != id) {
        throw std::invalid_argument(
            "first-playable content chains are not indexed by chain id");
    }
    return content;
}

// Programmatically constructed content must pass the same baseline checks as
// parsed content before it is admitted to authoritative simulation state.
// @implements spec/data/content-schema.md Validation
void validateFirstPlayableContent(const FirstPlayableContent& content);

// @implements spec/data/content-schema.md Validation
[[nodiscard]] FirstPlayableContent parseFirstPlayableContent(std::string_view json);
[[nodiscard]] FirstPlayableContent loadFirstPlayableContent(
    const std::filesystem::path& path);

}  // namespace konbini::sim
