#include "konbini/sim/population_change_system.h"
#include <algorithm>
#include <stdexcept>
namespace konbini::sim {
// @implements spec/feature/phase-1-game-loop.md Adjustable baseline
void applyPopulationLoss(PopulationCellTable& cells, const FacilityId facility,
                         const std::uint32_t percent) {
    if (percent > 100) { throw std::invalid_argument("invalid population loss percent"); }
    for (std::size_t i = 0; i < cells.size(); ++i) {
        const auto cell = cells.row(i);
        if (cell.facilityId != facility) { continue; }
        const auto loss = static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(cell.population) * percent / 100);
        cells.setPopulation(i, cell.population - loss);
    }
}
void recoverServedPopulation(PopulationCellTable& cells, const StoreTable& stores,
                            const Phase1Content& rules) {
    for (std::size_t i = 0; i < cells.size(); ++i) {
        const auto cell = cells.row(i);
        if (!cell.assignedStore) { continue; }
        const auto store = stores.find(*cell.assignedStore);
        if (!store || !stores.row(*store).isActive) { continue; }
        const auto recovered = std::min(rules.populationRecoveryPerPeriod, cell.capacity - cell.population);
        cells.setPopulation(i, cell.population + recovered);
    }
}
}  // namespace konbini::sim
