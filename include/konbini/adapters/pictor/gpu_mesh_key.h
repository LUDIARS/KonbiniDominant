#pragma once

#include <compare>
#include <cstdint>

#include "konbini/render/presentation_draw.h"
#include "konbini/sim/figmentum_facility_key.h"

// @implements spec/interface/pictor-rendering.md Presentation objects

namespace konbini::adapters::pictor {

// Which key space a GPU mesh belongs to. Figmentum facility keys span every
// uint64 value, so presentation meshes get their own domain instead of a
// reserved key range.
enum class GpuMeshDomain : std::uint8_t {
    Facility = 1,
    Presentation,
};

struct GpuMeshKey {
    GpuMeshDomain domain = GpuMeshDomain::Facility;
    std::uint64_t value = 0;

    [[nodiscard]] static constexpr GpuMeshKey facility(
        const sim::FigmentumFacilityKey key) noexcept {
        return {GpuMeshDomain::Facility, key.value()};
    }

    [[nodiscard]] static constexpr GpuMeshKey presentation(
        const render::PresentationMeshKey key) noexcept {
        return {GpuMeshDomain::Presentation,
                (static_cast<std::uint64_t>(key.kind) << 32U) | key.index};
    }

    // Facility key 0 is Figmentum's reserved "no key"; presentation keys are
    // valid once their kind is set.
    [[nodiscard]] constexpr bool isValid() const noexcept {
        return domain == GpuMeshDomain::Facility ? value != 0
                                                 : (value >> 32U) != 0;
    }

    auto operator<=>(const GpuMeshKey&) const = default;
};

}  // namespace konbini::adapters::pictor
