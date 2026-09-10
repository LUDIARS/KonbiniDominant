#include "konbini/app/fixed_step_driver.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

// @implements spec/interface/ergo-runtime.md Frame / simulation

namespace konbini::app {

FixedStepDriver::FixedStepDriver(
    const std::uint32_t ticksPerSecond, const std::uint32_t maxTicksPerFrame)
    : maxTicksPerFrame_(maxTicksPerFrame) {
    if (ticksPerSecond == 0) {
        throw std::invalid_argument(
            "fixed step driver requires a non-zero tick rate");
    }
    if (maxTicksPerFrame == 0) {
        throw std::invalid_argument(
            "fixed step driver requires a non-zero catch-up limit");
    }
    fixedDeltaSeconds_ = 1.0 / static_cast<double>(ticksPerSecond);
}

// @implements spec/design.md 5. Tick と決定性
FixedStepPlan FixedStepDriver::advance(const double renderDtSeconds, const double timeScale) {
    if (!std::isfinite(renderDtSeconds) || renderDtSeconds < 0.0) {
        throw std::invalid_argument(
            "fixed step driver requires a finite non-negative render delta");
    }

    if(!std::isfinite(timeScale) || timeScale<1.0 || timeScale>100.0)
        throw std::invalid_argument("time scale must be finite and between 1 and 100");
    accumulatedSeconds_ += timeScale * (renderDtSeconds > kMaxRenderDeltaSeconds
        ? kMaxRenderDeltaSeconds : renderDtSeconds);

    // Count whole ticks once: repeated subtraction can turn 0.25 / 0.01
    // into 24 ticks through rounding. Allow only the adjacent representable
    // quotient at an exact tick boundary.
    const double wholeTicks = std::floor(std::nextafter(
        accumulatedSeconds_ / fixedDeltaSeconds_,
        std::numeric_limits<double>::infinity()));
    if (wholeTicks > std::numeric_limits<std::uint32_t>::max()) {
        throw std::overflow_error("fixed step tick count overflow");
    }
    const auto availableTicks = static_cast<std::uint32_t>(wholeTicks);
    accumulatedSeconds_ = std::max(
        0.0, accumulatedSeconds_ - wholeTicks * fixedDeltaSeconds_);
    FixedStepPlan plan;
    plan.tickCount = std::min(availableTicks, maxTicksPerFrame_);
    plan.droppedTicks = availableTicks - plan.tickCount;
    return plan;
}

double FixedStepDriver::fixedDeltaSeconds() const noexcept {
    return fixedDeltaSeconds_;
}

double FixedStepDriver::accumulatedSeconds() const noexcept {
    return accumulatedSeconds_;
}

std::uint32_t FixedStepDriver::maxTicksPerFrame() const noexcept {
    return maxTicksPerFrame_;
}

double FixedStepDriver::drain() noexcept {
    const double drained = accumulatedSeconds_;
    accumulatedSeconds_ = 0.0;
    return drained;
}

}  // namespace konbini::app
