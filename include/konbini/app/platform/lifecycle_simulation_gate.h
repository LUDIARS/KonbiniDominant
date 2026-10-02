#pragma once

#include <optional>

#include "konbini/app/fixed_step_driver.h"
#include "konbini/app/platform/lifecycle_state.h"
#include "konbini/sim/player_command.h"

// @implements spec/interface/mobile-platform.md Lifecycle

namespace konbini::app {

// Plans this frame's fixed ticks. While the lifecycle holds the simulation
// the accumulator is drained, so time spent paused or in the background is
// never replayed as catch-up ticks on resume.
// @implements spec/interface/mobile-platform.md Lifecycle
[[nodiscard]] FixedStepPlan planLifecycleTicks(
    const LifecycleState& lifecycle, FixedStepDriver& driver,
    double renderDtSeconds, double timeScale = 1.0);

// New game commands are refused, not buffered, while the lifecycle holds
// the simulation: a tap made before a pause must not land after resume.
// @implements spec/interface/mobile-platform.md Lifecycle
[[nodiscard]] std::optional<sim::PlayerCommand> admitGameCommand(
    const LifecycleState& lifecycle, const sim::PlayerCommand& command);

}  // namespace konbini::app
