#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "konbini/app/platform/memory_pressure.h"
#include "konbini/city/facility_geometry.h"

// @implements spec/interface/mobile-platform.md Surface and renderer

namespace konbini::app {

// Bumped whenever a declared profile changes a value below.
inline constexpr std::uint32_t kMobileGraphicsProfileVersion = 1;

// Pictor's mobile preset names. The game declares what each one means for
// its own world pass instead of inheriting a desktop default.
enum class MobileGraphicsProfileId : std::uint8_t {
    MobileHigh,
    MobileLow,
};

// One versioned mobile profile. The world color / depth formats are the ones
// `WorldSceneTargets` capability-checks at device init; an unsupported device
// fails that check instead of silently getting another format.
// @implements spec/interface/mobile-platform.md Surface and renderer
struct MobileGraphicsProfile {
    MobileGraphicsProfileId id = MobileGraphicsProfileId::MobileHigh;
    std::string_view name;
    std::uint32_t version = kMobileGraphicsProfileVersion;
    std::string_view worldColorFormat;
    std::string_view worldDepthFormat;
    double renderScale = 1.0;
    // Facilities are polygonized once at boot at this coarser level, never
    // in the frame loop.
    city::FacilityMeshDetail facilityMesh;
};

[[nodiscard]] const MobileGraphicsProfile& mobileGraphicsProfile(
    MobileGraphicsProfileId id) noexcept;

// The explicit boot-time choice and what it was based on. `thermal` is empty
// when the OS offers no thermal status (Android API < 30).
struct MobileGraphicsProfileSelection {
    MobileGraphicsProfileId profile = MobileGraphicsProfileId::MobileHigh;
    std::optional<ThermalLevel> thermal;
};

// Serious / critical thermal state at boot selects MobileLow; nominal, fair or
// unknown selects MobileHigh. The rule is fixed: it never reads device names.
// @implements spec/interface/mobile-platform.md Surface and renderer
[[nodiscard]] MobileGraphicsProfileSelection selectMobileGraphicsProfile(
    std::optional<ThermalLevel> thermal) noexcept;

[[nodiscard]] std::string_view thermalLevelName(ThermalLevel level) noexcept;

// One diagnostic line naming the profile, its version, formats, render scale,
// facility mesh level and the thermal input of the choice.
[[nodiscard]] std::string describeMobileGraphicsProfileSelection(
    const MobileGraphicsProfileSelection& selection);

}  // namespace konbini::app
