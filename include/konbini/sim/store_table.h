#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "konbini/sim/chain_id.h"
#include "konbini/sim/entity_id.h"
#include "konbini/sim/math_types.h"

// @implements spec/data/world-state.md StoreTable

namespace konbini::sim {

struct StoreRow {
    StoreId id{};
    FacilityId facilityId{};
    ChainId chain = ChainId::Losan;
    std::uint32_t dimension = 0;
    Vec3 positionMeters{};
    double zocRadiusMeters = 0.0;
    std::uint64_t capturedPopulation = 0;
    bool isActive = true;
    std::uint32_t revenuePermille = 1000;
    std::uint32_t verticalSlot = 0, faith = 0;
    bool isAntiStore = false;
};

class StoreTable {
public:
    void append(const StoreRow& row);

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] StoreRow row(std::size_t index) const;
    [[nodiscard]] std::optional<std::size_t> find(StoreId id) const noexcept;
    [[nodiscard]] bool hasActiveStoreAt(FacilityId facilityId) const noexcept;

    [[nodiscard]] std::optional<std::size_t> findAt(FacilityId facility, std::uint32_t slot) const noexcept;
    [[nodiscard]] std::uint32_t firstEmptySlot(FacilityId facility, std::uint32_t limit) const noexcept;
    void setZocRadius(std::size_t index, double radius);
    void setFaith(std::size_t index, std::uint32_t faith);
    void markAntiStore(std::size_t index);
    void clearCapturedPopulation() noexcept;
    void addCapturedPopulation(std::size_t denseIndex, std::uint32_t population);
    void setRevenuePermille(std::size_t denseIndex, std::uint32_t value);
    [[nodiscard]] bool deactivate(StoreId id) noexcept;

private:
    static constexpr std::size_t kMissing = static_cast<std::size_t>(-1);

    std::vector<StoreId> ids_;
    std::vector<FacilityId> facilityIds_;
    std::vector<ChainId> chains_;
    std::vector<std::uint32_t> dimensions_;
    std::vector<Vec3> positionsMeters_;
    std::vector<double> zocRadiiMeters_;
    std::vector<std::uint64_t> capturedPopulations_;
    std::vector<std::uint8_t> isActive_;
    std::vector<std::uint32_t> revenuePermille_;
    std::vector<std::uint32_t> verticalSlots_, faiths_;
    std::vector<std::uint8_t> antiStores_;
    std::vector<std::size_t> sparseIndices_;
};

}  // namespace konbini::sim
