#include "konbini/sim/population_initializer.h"

#include <limits>
#include <stdexcept>
#include <utility>

// @implements spec/feature/economy-and-population.md 人口
// @implements spec/design.md 5. Tick と決定性

namespace konbini::sim {

// PopulationCell は lot / block 単位で作る。counter RNG の stableId は
// Figmentum の facility key なので、facility の走査順や生成回数が変わっても
// 同じ world seed から同じ人口が出る。
// @implements spec/feature/economy-and-population.md 人口
// @implements spec/design.md 5. Tick と決定性
void initializePopulationCells(
    const FacilityTable& facilities, const FirstPlayableContent& content,
    const std::uint64_t worldSeed, PopulationCellTable& populationCells,
    GenerationalIdPool<PopulationCellId>& populationCellIds) {
    if (populationCells.size() != 0) {
        throw std::invalid_argument(
            "population cells must be empty before initialization");
    }
    if (content.population.randomPopulationCount == 0) {
        throw std::invalid_argument("population random range must be positive");
    }
    const std::uint64_t maximumPopulation =
        static_cast<std::uint64_t>(content.population.basePopulation) +
        static_cast<std::uint64_t>(
            content.population.randomPopulationCount - 1U);
    if (maximumPopulation > std::numeric_limits<std::uint32_t>::max()) {
        throw std::overflow_error("population cell value overflow");
    }

    PopulationCellTable stagedCells = populationCells;
    GenerationalIdPool<PopulationCellId> stagedIds = populationCellIds;
    for (std::size_t index = 0; index < facilities.size(); ++index) {
        const FacilityRow facility = facilities.row(index);
        if (!facility.isBuildable) {
            continue;
        }
        const std::uint64_t randomValue = counterRandom({
            .worldSeed = worldSeed,
            .stream = RandomStreamId::FirstPlayablePopulation,
            .tick = 0,
            .stableId = facility.figmentumKey.value(),
            .ordinal = 0,
        });
        const std::uint64_t population =
            static_cast<std::uint64_t>(content.population.basePopulation) +
            randomValue % content.population.randomPopulationCount;
        stagedCells.append({
            .id = stagedIds.acquire(),
            .facilityId = facility.id,
            .dimension = facility.dimension,
            .positionMeters = facility.positionMeters,
            .population = static_cast<std::uint32_t>(population),
        });
    }
    populationCells = std::move(stagedCells);
    populationCellIds = std::move(stagedIds);
}

}  // namespace konbini::sim
