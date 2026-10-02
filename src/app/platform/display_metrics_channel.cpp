#include "konbini/app/platform/display_metrics_channel.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

// @implements spec/interface/mobile-platform.md Lifecycle

namespace konbini::app {

bool DisplayMetricsChange::extentChanged() const noexcept {
    return !previous || previous->extentPixels != current.extentPixels;
}

bool DisplayMetricsChange::densityChanged() const noexcept {
    return !previous || previous->density != current.density;
}

bool DisplayMetricsChange::safeAreaChanged() const noexcept {
    return !previous || previous->safeArea != current.safeArea;
}

bool DisplayMetricsChange::orientationChanged() const noexcept {
    return !previous || previous->orientation() != current.orientation();
}

DisplayMetricsChannel::SubscriptionId DisplayMetricsChannel::subscribe(
    Listener listener) {
    if (!listener) {
        throw std::invalid_argument("display metrics listener must be callable");
    }
    const SubscriptionId id = nextId_++;
    subscriptions_.push_back({id, std::move(listener)});
    return id;
}

void DisplayMetricsChannel::unsubscribe(const SubscriptionId id) noexcept {
    std::erase_if(subscriptions_, [id](const Subscription& subscription) {
        return subscription.id == id;
    });
}

// @implements spec/interface/mobile-platform.md Lifecycle
bool DisplayMetricsChannel::publish(const DisplayMetrics& metrics) {
    validateDisplayMetrics(metrics);
    if (current_ == metrics) {
        return false;
    }
    const DisplayMetricsChange change{current_, metrics};
    current_ = metrics;
    // Iterate a copy: a listener may unsubscribe itself while notified.
    const std::vector<Subscription> subscriptions = subscriptions_;
    for (const Subscription& subscription : subscriptions) {
        subscription.listener(change);
    }
    return true;
}

const std::optional<DisplayMetrics>& DisplayMetricsChannel::current()
    const noexcept {
    return current_;
}

}  // namespace konbini::app
