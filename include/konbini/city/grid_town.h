// @implements spec/feature/grid-town-and-vector-ui.md Grid town
#pragma once
#include <string_view>
#include "konbini/city/facility_geometry.h"

namespace konbini::city {
inline constexpr std::string_view kGridTownRevision = "konbini-grid-town-v1";
inline constexpr int kGridTownSide = 16;
inline constexpr double kGridCellMeters = 12.0;
void validateGridTownCells(const CityManifest& manifest);
[[nodiscard]] GeneratedCity makeGridTown(sim::GenerationalIdPool<sim::FacilityId>& ids);
[[nodiscard]] inline bool isGridTown(const CityManifest& manifest) noexcept {
    return manifest.generatorRevision == kGridTownRevision;
}
}
