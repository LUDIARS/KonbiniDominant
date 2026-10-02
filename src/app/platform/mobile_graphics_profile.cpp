#include "konbini/app/platform/mobile_graphics_profile.h"

#include <sstream>

// @implements spec/interface/mobile-platform.md Surface and renderer

namespace konbini::app {
namespace {

// Both profiles keep the desktop RGBA16F + D32 world target: the composite
// pass has no other format path yet, so a coarser profile only lowers the
// facility mesh level. Desktop meshes at resolution 24 / LOD 0.
constexpr MobileGraphicsProfile kMobileHigh{
    .id = MobileGraphicsProfileId::MobileHigh,
    .name = "MobileHigh",
    .version = kMobileGraphicsProfileVersion,
    .worldColorFormat = "R16G16B16A16_SFLOAT",
    .worldDepthFormat = "D32_SFLOAT",
    .renderScale = 1.0,
    .facilityMesh = {.polygonizeResolution = 16, .lod = 1},
};

constexpr MobileGraphicsProfile kMobileLow{
    .id = MobileGraphicsProfileId::MobileLow,
    .name = "MobileLow",
    .version = kMobileGraphicsProfileVersion,
    .worldColorFormat = "R16G16B16A16_SFLOAT",
    .worldDepthFormat = "D32_SFLOAT",
    .renderScale = 1.0,
    .facilityMesh = {.polygonizeResolution = 12, .lod = 2},
};

}  // namespace

const MobileGraphicsProfile& mobileGraphicsProfile(
    const MobileGraphicsProfileId id) noexcept {
    return id == MobileGraphicsProfileId::MobileLow ? kMobileLow : kMobileHigh;
}

// @implements spec/interface/mobile-platform.md Surface and renderer
MobileGraphicsProfileSelection selectMobileGraphicsProfile(
    const std::optional<ThermalLevel> thermal) noexcept {
    const bool throttled = thermal.has_value() &&
                           (*thermal == ThermalLevel::Serious ||
                            *thermal == ThermalLevel::Critical);
    return {
        .profile = throttled ? MobileGraphicsProfileId::MobileLow
                             : MobileGraphicsProfileId::MobileHigh,
        .thermal = thermal,
    };
}

std::string_view thermalLevelName(const ThermalLevel level) noexcept {
    switch (level) {
        case ThermalLevel::Nominal:
            return "nominal";
        case ThermalLevel::Fair:
            return "fair";
        case ThermalLevel::Serious:
            return "serious";
        case ThermalLevel::Critical:
            return "critical";
    }
    return "unknown";
}

std::string describeMobileGraphicsProfileSelection(
    const MobileGraphicsProfileSelection& selection) {
    const MobileGraphicsProfile& profile =
        mobileGraphicsProfile(selection.profile);
    std::ostringstream line;
    line << "graphics-profile=" << profile.name << " version="
         << profile.version << " world-color=" << profile.worldColorFormat
         << " world-depth=" << profile.worldDepthFormat
         << " render-scale=" << profile.renderScale
         << " facility-polygonize=" << profile.facilityMesh.polygonizeResolution
         << " facility-lod=" << profile.facilityMesh.lod << " thermal="
         << (selection.thermal ? thermalLevelName(*selection.thermal)
                               : std::string_view("unavailable"));
    return line.str();
}

}  // namespace konbini::app
