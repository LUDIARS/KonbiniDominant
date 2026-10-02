#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include "konbini/app/platform/display_metrics.h"
#include "konbini/app/platform/display_metrics_channel.h"

#include "../../check.h"

// @implements spec/test/verification-strategy.md 1. Data / ID unit tests
// @implements spec/interface/mobile-platform.md Lifecycle

namespace {

using konbini::app::DisplayMetrics;
using konbini::app::DisplayMetricsChange;
using konbini::app::DisplayMetricsChannel;
using konbini::app::DisplayOrientation;
using konbini::app::validateDisplayMetrics;

[[nodiscard]] DisplayMetrics portrait() {
    DisplayMetrics metrics;
    metrics.extentPixels = {1080, 2400};
    metrics.density = 3.0;
    metrics.safeArea = {0, 96, 0, 48};
    return metrics;
}

[[nodiscard]] DisplayMetrics landscape() {
    DisplayMetrics metrics;
    metrics.extentPixels = {2400, 1080};
    metrics.density = 3.0;
    metrics.safeArea = {96, 0, 48, 0};
    return metrics;
}

void metricsValueDerivesLayout() {
    CHECK(portrait().orientation() == DisplayOrientation::Portrait);
    CHECK(landscape().orientation() == DisplayOrientation::Landscape);
    CHECK(portrait().usableExtentPixels().width == 1080);
    CHECK(portrait().usableExtentPixels().height == 2400 - 96 - 48);
    DisplayMetrics square = portrait();
    square.extentPixels = {800, 800};
    CHECK(square.orientation() == DisplayOrientation::Landscape);
}

void metricsValidationRejectsUnusableDisplays() {
    CHECK_NO_THROW(validateDisplayMetrics(portrait()));

    DisplayMetrics empty = portrait();
    empty.extentPixels.height = 0;
    CHECK_THROWS(std::invalid_argument, validateDisplayMetrics(empty));

    DisplayMetrics zeroDensity = portrait();
    zeroDensity.density = 0.0;
    CHECK_THROWS(std::invalid_argument, validateDisplayMetrics(zeroDensity));

    DisplayMetrics nanDensity = portrait();
    nanDensity.density = std::numeric_limits<double>::quiet_NaN();
    CHECK_THROWS(std::invalid_argument, validateDisplayMetrics(nanDensity));

    DisplayMetrics covered = portrait();
    covered.safeArea = {540, 0, 540, 0};
    CHECK_THROWS(std::invalid_argument, validateDisplayMetrics(covered));

    // Huge insets must not wrap around to a "usable" width.
    DisplayMetrics wrapping = portrait();
    wrapping.safeArea = {std::numeric_limits<std::uint32_t>::max(), 0, 2, 0};
    CHECK_THROWS(std::invalid_argument, validateDisplayMetrics(wrapping));
}

void channelNotifiesRenderThenUiOnChange() {
    DisplayMetricsChannel channel;
    std::vector<int> order;
    std::vector<DisplayMetricsChange> renderChanges;
    const auto render = channel.subscribe([&](const DisplayMetricsChange& change) {
        order.push_back(1);
        renderChanges.push_back(change);
    });
    static_cast<void>(channel.subscribe([&](const DisplayMetricsChange&) { order.push_back(2); }));

    CHECK(channel.publish(portrait()));
    CHECK(order == (std::vector<int>{1, 2}));
    CHECK(!renderChanges.back().previous.has_value());
    CHECK(renderChanges.back().orientationChanged());

    // Same metrics again: no rebuild for a repeated OS callback.
    CHECK(!channel.publish(portrait()));
    CHECK(order.size() == 2);

    CHECK(channel.publish(landscape()));
    const DisplayMetricsChange& rotation = renderChanges.back();
    CHECK(rotation.previous == portrait());
    CHECK(rotation.current == landscape());
    CHECK(rotation.extentChanged());
    CHECK(rotation.orientationChanged());
    CHECK(rotation.safeAreaChanged());
    CHECK(!rotation.densityChanged());
    CHECK(channel.current() == landscape());

    channel.unsubscribe(render);
    DisplayMetrics denser = landscape();
    denser.density = 3.5;
    CHECK(channel.publish(denser));
    CHECK(renderChanges.size() == 2);
    CHECK(order.back() == 2);
}

void channelRejectsInvalidInput() {
    DisplayMetricsChannel channel;
    CHECK_THROWS(std::invalid_argument,
                 static_cast<void>(channel.subscribe(DisplayMetricsChannel::Listener{})));
    DisplayMetrics invalid = portrait();
    invalid.density = -1.0;
    CHECK_THROWS(std::invalid_argument, static_cast<void>(channel.publish(invalid)));
    CHECK(!channel.current().has_value());
}

}  // namespace

int main() {
    metricsValueDerivesLayout();
    metricsValidationRejectsUnusableDisplays();
    channelNotifiesRenderThenUiOnChange();
    channelRejectsInvalidInput();
    return konbini::test::summarize("konbini_display_metrics_tests");
}
