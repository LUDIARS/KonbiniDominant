#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include "konbini/app/platform/display_metrics.h"

// @implements spec/interface/mobile-platform.md Lifecycle

namespace konbini::app {

struct DisplayMetricsChange {
    // Empty on the first publish: render / UI have no layout yet.
    std::optional<DisplayMetrics> previous;
    DisplayMetrics current;

    [[nodiscard]] bool extentChanged() const noexcept;
    [[nodiscard]] bool densityChanged() const noexcept;
    [[nodiscard]] bool safeAreaChanged() const noexcept;
    [[nodiscard]] bool orientationChanged() const noexcept;
};

// Fans a display change out to render and UI on the app owner thread.
// Subscribers are notified in subscription order, and only when the metrics
// actually differ, so a host that repeats the same callback does not force a
// swapchain rebuild.
// @implements spec/interface/mobile-platform.md Lifecycle
class DisplayMetricsChannel {
public:
    using Listener = std::function<void(const DisplayMetricsChange&)>;
    using SubscriptionId = std::uint32_t;

    [[nodiscard]] SubscriptionId subscribe(Listener listener);
    void unsubscribe(SubscriptionId id) noexcept;

    // Validates, then notifies. Returns false when nothing changed.
    bool publish(const DisplayMetrics& metrics);

    [[nodiscard]] const std::optional<DisplayMetrics>& current()
        const noexcept;

private:
    struct Subscription {
        SubscriptionId id = 0;
        Listener listener;
    };

    std::vector<Subscription> subscriptions_;
    SubscriptionId nextId_ = 1;
    std::optional<DisplayMetrics> current_;
};

}  // namespace konbini::app
