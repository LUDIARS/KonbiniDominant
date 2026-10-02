#pragma once

#include <cstdint>

#include "konbini/render/viewport_extent.h"

// @implements spec/interface/mobile-platform.md Required boundaries

namespace konbini::app {

enum class DisplayOrientation : std::uint8_t {
    Portrait,
    Landscape,
};

// Pixels the OS reserves (notch, home indicator, rounded corners).
struct SafeAreaInsets {
    std::uint32_t left = 0;
    std::uint32_t top = 0;
    std::uint32_t right = 0;
    std::uint32_t bottom = 0;

    friend bool operator==(const SafeAreaInsets&, const SafeAreaInsets&) =
        default;
};

// One normalized description of the drawable area, filled by the platform
// host. It is a layout input for render / UI only: graphics profile, render
// scale and safe area never enter the canonical save.
// @implements spec/interface/mobile-platform.md Required boundaries
struct DisplayMetrics {
    render::ViewportExtent extentPixels;
    double density = 1.0;
    SafeAreaInsets safeArea;

    // A square extent counts as landscape so the layout never flips on it.
    [[nodiscard]] DisplayOrientation orientation() const noexcept;
    // The extent left after the safe area. Valid metrics keep it non-empty.
    [[nodiscard]] render::ViewportExtent usableExtentPixels() const noexcept;

    friend bool operator==(const DisplayMetrics&, const DisplayMetrics&) =
        default;
};

// Throws `std::invalid_argument` for an empty extent, a non-finite or
// non-positive density, or a safe area that leaves no usable pixels.
void validateDisplayMetrics(const DisplayMetrics& metrics);

}  // namespace konbini::app
