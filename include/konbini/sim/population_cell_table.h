#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "konbini/sim/chain_id.h"
#include "konbini/sim/entity_id.h"
#include "konbini/sim/math_types.h"

// @implements spec/data/world-state.md PopulationCellTable

namespace konbini::sim {

struct PopulationCellRow {
    PopulationCellId id{};
    FacilityId facilityId{};
    std::uint32_t dimension = 0;
    Vec3 positionMeters{};
    std::uint32_t population = 0;
    std::optional<StoreId> assignedStore;
    std::optional<ChainId> preferredChain;
    std::uint32_t capacity = 0;
};

class PopulationCellTable {
public:
    void append(const PopulationCellRow& row);

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] PopulationCellRow row(std::size_t index) const;
    void assign(std::size_t denseIndex, std::optional<StoreId> store,
                std::optional<ChainId> chain);
    void setPopulation(std::size_t denseIndex, std::uint32_t population);

private:
    std::vector<PopulationCellId> ids_;
    std::vector<FacilityId> facilityIds_;
    std::vector<std::uint32_t> dimensions_;
    std::vector<Vec3> positionsMeters_;
    std::vector<std::uint32_t> populations_;
    std::vector<std::uint32_t> capacities_;
    std::vector<StoreId> assignedStores_;
    std::vector<ChainId> preferredChains_;
    std::vector<std::uint8_t> hasAssignment_;
};

}  // namespace konbini::sim
