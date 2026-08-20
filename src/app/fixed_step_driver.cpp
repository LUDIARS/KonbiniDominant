#include "konbini/app/fixed_step_driver.h"

#include <cmath>
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
FixedStepPlan FixedStepDriver::advance(const double renderDtSeconds) {
    if (!std::isfinite(renderDtSeconds) || renderDtSeconds < 0.0) {
        throw std::invalid_argument(
            "fixed step driver requires a finite non-negative render delta");
    }

    accumulatedSeconds_ +=
        renderDtSeconds > kMaxRenderDeltaSeconds ? kMaxRenderDeltaSeconds
                                                 : renderDtSeconds;

    FixedStepPlan plan;
    while (accumulatedSeconds_ >= fixedDeltaSeconds_) {
        accumulatedSeconds_ -= fixedDeltaSeconds_;
        if (plan.tickCount < maxTicksPerFrame_) {
            ++plan.tickCount;
        } else {
            // 上限を超えた分は「進めなかった tick」として報告する。
            // accumulator に残したままにすると次フレーム以降も上限に張り付き、
            // 復帰できなくなる。
            ++plan.droppedTicks;
        }
    }
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
