// @implements spec/feature/ui-ux.md Smartphone interaction
#include "konbini/app/hud_layout_metrics.h"

#include <cmath>
#include <stdexcept>

namespace konbini::app {

HudLayoutMetrics hudLayoutFromDisplay(
    const DisplayMetrics& display, const double userUiScale) {
    validateDisplayMetrics(display);
    if (!std::isfinite(userUiScale) || userUiScale < kMinUserUiScale ||
        userUiScale > kMaxUserUiScale) {
        throw std::invalid_argument("user UI scale is outside the supported range");
    }
    return {.extentPixels = display.extentPixels,
            .safeArea = display.safeArea,
            .density = display.density,
            .userUiScale = userUiScale};
}

HudSafeRect hudSafeRect(const HudLayoutMetrics& metrics) noexcept {
    const auto& extent = metrics.extentPixels;
    const auto& inset = metrics.safeArea;
    const auto clampSum = [](std::uint32_t a, std::uint32_t b, std::uint32_t limit) {
        const auto sum = static_cast<std::uint64_t>(a) + b;
        return sum >= limit ? limit : static_cast<std::uint32_t>(sum);
    };
    const auto horizontal = clampSum(inset.left, inset.right, extent.width);
    const auto vertical = clampSum(inset.top, inset.bottom, extent.height);
    HudSafeRect rect;
    rect.x = static_cast<float>(inset.left < extent.width ? inset.left : extent.width);
    rect.y = static_cast<float>(inset.top < extent.height ? inset.top : extent.height);
    rect.width = static_cast<float>(extent.width - horizontal);
    rect.height = static_cast<float>(extent.height - vertical);
    return rect;
}

}  // namespace konbini::app
