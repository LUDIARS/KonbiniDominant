#pragma once

#include <cstdint>

#include "konbini/city/derived_geometry_cache_policy.h"

// @implements spec/interface/mobile-platform.md Lifecycle

namespace konbini::app {

enum class MemoryPressureLevel : std::uint8_t {
    Normal,
    Moderate,
    Critical,
};

// Thermal state is forwarded to the render profile owner unchanged; the
// simulation rule never reads it.
enum class ThermalLevel : std::uint8_t {
    Nominal,
    Fair,
    Serious,
    Critical,
};

// Staged eviction: moderate pressure drops only geometry nobody is drawing,
// critical pressure drops every regenerable entry.
// @implements spec/interface/mobile-platform.md Lifecycle
[[nodiscard]] city::GeometryEvictionScope geometryEvictionScope(
    MemoryPressureLevel level) noexcept;

}  // namespace konbini::app
