#pragma once

#include "konbini/app/platform/display_metrics.h"
#include "konbini/render/viewport_extent.h"

// @implements spec/feature/ui-ux.md Smartphone interaction
// @implements spec/interface/mobile-platform.md Input and UI

namespace konbini::app {

// The layout input of the HUD: drawable extent, the OS-reserved edges,
// display density and the player's UI scale setting. Presentation-only; a
// rotation or resize changes these values and nothing else.
struct HudLayoutMetrics {
    render::ViewportExtent extentPixels{};
    SafeAreaInsets safeArea{};
    // Physical pixels per layout pixel (Windows content scale, Android
    // density, iOS contentScaleFactor).
    double density = 1.0;
    // Accessibility UI scale, multiplied on top of the density.
    double userUiScale = 1.0;

    friend bool operator==(const HudLayoutMetrics&, const HudLayoutMetrics&) = default;
};

// Bounds the player UI scale setting may take.
inline constexpr double kMinUserUiScale = 0.75;
inline constexpr double kMaxUserUiScale = 1.5;

// Throws `std::invalid_argument` for invalid display metrics (see
// `validateDisplayMetrics`) or a user UI scale outside
// [kMinUserUiScale, kMaxUserUiScale].
[[nodiscard]] HudLayoutMetrics hudLayoutFromDisplay(
    const DisplayMetrics& display, double userUiScale = 1.0);

// The rect left after the safe area, in framebuffer pixels. An inset larger
// than the extent collapses the rect to empty instead of wrapping.
struct HudSafeRect {
    float x = 0.0F, y = 0.0F, width = 0.0F, height = 0.0F;
};
[[nodiscard]] HudSafeRect hudSafeRect(const HudLayoutMetrics& metrics) noexcept;

}  // namespace konbini::app
