#include "konbini/app/platform/display_metrics.h"

#include <cmath>
#include <cstdint>
#include <stdexcept>

// @implements spec/interface/mobile-platform.md Required boundaries

namespace konbini::app {
namespace {

// Inset sums are computed in 64 bits so two large insets cannot wrap.
[[nodiscard]] std::uint64_t insetSum(
    const std::uint32_t first, const std::uint32_t second) noexcept {
    return static_cast<std::uint64_t>(first) + second;
}

}  // namespace

DisplayOrientation DisplayMetrics::orientation() const noexcept {
    return extentPixels.height > extentPixels.width
               ? DisplayOrientation::Portrait
               : DisplayOrientation::Landscape;
}

render::ViewportExtent DisplayMetrics::usableExtentPixels() const noexcept {
    const std::uint64_t horizontal = insetSum(safeArea.left, safeArea.right);
    const std::uint64_t vertical = insetSum(safeArea.top, safeArea.bottom);
    render::ViewportExtent usable;
    if (horizontal < extentPixels.width) {
        usable.width =
            static_cast<std::uint32_t>(extentPixels.width - horizontal);
    }
    if (vertical < extentPixels.height) {
        usable.height =
            static_cast<std::uint32_t>(extentPixels.height - vertical);
    }
    return usable;
}

// @implements spec/interface/mobile-platform.md Required boundaries
void validateDisplayMetrics(const DisplayMetrics& metrics) {
    if (metrics.extentPixels.width == 0 || metrics.extentPixels.height == 0) {
        throw std::invalid_argument("display extent must not be empty");
    }
    if (!std::isfinite(metrics.density) || metrics.density <= 0.0) {
        throw std::invalid_argument("display density must be finite and positive");
    }
    const render::ViewportExtent usable = metrics.usableExtentPixels();
    if (usable.width == 0 || usable.height == 0) {
        throw std::invalid_argument("display safe area leaves no usable pixels");
    }
}

}  // namespace konbini::app
