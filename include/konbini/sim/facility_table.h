#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "konbini/sim/entity_id.h"
#include "konbini/sim/figmentum_facility_key.h"
#include "konbini/sim/math_types.h"

// @implements spec/data/world-state.md FacilityTable

namespace konbini::sim {

enum class FacilityState : std::uint8_t {
    Intact = 0,
    Replaced,
    Destroyed,
};

struct FacilityRow {
    FacilityId id{};
    FigmentumFacilityKey figmentumKey{};
    std::uint32_t dimension = 0;
    Vec3 positionMeters{};
    Bounds3 boundsMeters{};
    bool isBuildable = false;
    FacilityState state = FacilityState::Intact;
};

class FacilityTable {
public:
    void append(const FacilityRow& row);

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] FacilityRow row(std::size_t index) const;
    [[nodiscard]] std::optional<std::size_t> find(FacilityId id) const noexcept;
    [[nodiscard]] bool setState(FacilityId id, FacilityState state) noexcept;

private:
    static constexpr std::size_t kMissing = static_cast<std::size_t>(-1);

    std::vector<FacilityId> ids_;
    std::vector<FigmentumFacilityKey> figmentumKeys_;
    std::vector<std::uint32_t> dimensions_;
    std::vector<Vec3> positionsMeters_;
    std::vector<Bounds3> boundsMeters_;
    std::vector<std::uint8_t> isBuildable_;
    std::vector<FacilityState> states_;
    std::vector<std::size_t> sparseIndices_;
};

}  // namespace konbini::sim
