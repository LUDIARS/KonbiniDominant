#include "konbini/app/platform/lifecycle_simulation_gate.h"

// @implements spec/interface/mobile-platform.md Lifecycle

namespace konbini::app {

// @implements spec/interface/mobile-platform.md Lifecycle
FixedStepPlan planLifecycleTicks(
    const LifecycleState& lifecycle, FixedStepDriver& driver,
    const double renderDtSeconds, const double timeScale) {
    if (!lifecycle.advancesSimulation()) {
        static_cast<void>(driver.drain());
        return {};
    }
    return driver.advance(renderDtSeconds, timeScale);
}

// @implements spec/interface/mobile-platform.md Lifecycle
std::optional<sim::PlayerCommand> admitGameCommand(
    const LifecycleState& lifecycle, const sim::PlayerCommand& command) {
    if (!lifecycle.acceptsCommands()) {
        return std::nullopt;
    }
    return command;
}

}  // namespace konbini::app
