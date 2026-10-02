#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

#include "konbini/app/fixed_step_driver.h"
#include "konbini/app/platform/lifecycle_event.h"
#include "konbini/app/platform/lifecycle_event_queue.h"
#include "konbini/app/platform/lifecycle_simulation_gate.h"
#include "konbini/app/platform/lifecycle_state.h"
#include "konbini/app/platform/memory_pressure.h"
#include "konbini/app/platform/safe_checkpoint.h"
#include "konbini/sim/canonical_snapshot.h"
#include "konbini/sim/player_command.h"

#include "../../check.h"

// @implements spec/test/verification-strategy.md 1. Data / ID unit tests
// @implements spec/interface/mobile-platform.md Lifecycle

namespace {

using konbini::app::admitGameCommand;
using konbini::app::DisplayMetrics;
using konbini::app::FixedStepDriver;
using konbini::app::FixedStepPlan;
using konbini::app::geometryEvictionScope;
using konbini::app::LifecycleEffects;
using konbini::app::LifecycleEvent;
using konbini::app::LifecycleEventKind;
using konbini::app::LifecycleEventQueue;
using konbini::app::LifecycleQueueOverflow;
using konbini::app::LifecycleState;
using konbini::app::MemoryPressureLevel;
using konbini::app::planLifecycleTicks;
using konbini::app::SafeCheckpointLedger;
using konbini::app::ThermalLevel;
using konbini::city::GeometryEvictionScope;
using konbini::sim::CanonicalSnapshot;
using konbini::sim::PlayerCommand;
using konbini::sim::SelectChainCommand;

[[nodiscard]] DisplayMetrics phoneMetrics() {
    DisplayMetrics metrics;
    metrics.extentPixels = {1080, 2400};
    metrics.density = 3.0;
    metrics.safeArea = {0, 96, 0, 48};
    return metrics;
}

[[nodiscard]] CanonicalSnapshot snapshotWithHash(const std::uint64_t hash) {
    CanonicalSnapshot snapshot;
    snapshot.bytes = {std::byte{1}, std::byte{2}, std::byte{3}};
    snapshot.hash = hash;
    return snapshot;
}

[[nodiscard]] LifecycleState runningState() {
    LifecycleState state;
    static_cast<void>(state.apply(LifecycleEvent::surfaceAvailable(phoneMetrics())));
    return state;
}

void queuePreservesPushOrder() {
    LifecycleEventQueue queue(4);
    CHECK(queue.push(LifecycleEvent::pause()));
    CHECK(queue.push(LifecycleEvent::surfaceLost()));
    CHECK(queue.push(LifecycleEvent::resume()));
    CHECK(queue.pending() == 3);

    const std::vector<LifecycleEvent> drained = queue.drain();
    CHECK(drained.size() == 3);
    CHECK(drained[0].kind == LifecycleEventKind::Pause);
    CHECK(drained[1].kind == LifecycleEventKind::SurfaceLost);
    CHECK(drained[2].kind == LifecycleEventKind::Resume);
    CHECK(queue.pending() == 0);
    CHECK(queue.drain().empty());
}

void queueAcceptsEventsFromCallbackThread() {
    LifecycleEventQueue queue(8);
    std::thread callback([&queue] {
        static_cast<void>(queue.push(LifecycleEvent::enterBackground()));
        static_cast<void>(queue.push(LifecycleEvent::enterForeground()));
    });
    callback.join();
    const std::vector<LifecycleEvent> drained = queue.drain();
    CHECK(drained.size() == 2);
    CHECK(drained[0].kind == LifecycleEventKind::EnterBackground);
    CHECK(drained[1].kind == LifecycleEventKind::EnterForeground);
}

void queueDrainIsOwnerThreadOnly() {
    LifecycleEventQueue queue(2);
    bool rejected = false;
    std::thread other([&queue, &rejected] {
        try {
            static_cast<void>(queue.drain());
        } catch (const std::logic_error&) {
            rejected = true;
        }
    });
    other.join();
    CHECK(rejected);
}

void queueOverflowFailsFastOnOwner() {
    CHECK_THROWS(std::invalid_argument, LifecycleEventQueue(0));

    LifecycleEventQueue queue(1);
    CHECK(queue.push(LifecycleEvent::pause()));
    CHECK(!queue.push(LifecycleEvent::resume()));
    // Even after room appears, the hole in the sequence stays reported.
    CHECK(!queue.push(LifecycleEvent::resume()));
    CHECK_THROWS(LifecycleQueueOverflow, static_cast<void>(queue.drain()));
}

void pauseHoldsTicksAndCommands() {
    LifecycleState state = runningState();
    CHECK(state.advancesSimulation());
    CHECK(state.acceptsCommands());

    FixedStepDriver driver(10, 5);
    const FixedStepPlan running = planLifecycleTicks(state, driver, 0.25);
    CHECK(running.tickCount == 2);

    static_cast<void>(state.apply(LifecycleEvent::pause()));
    CHECK(!state.advancesSimulation());
    CHECK(!state.acceptsCommands());
    // Pause alone keeps the last frame presentable.
    CHECK(state.maySubmitGpu());

    const PlayerCommand command = SelectChainCommand{};
    CHECK(!admitGameCommand(state, command).has_value());
    const FixedStepPlan paused = planLifecycleTicks(state, driver, 0.25);
    CHECK(paused.tickCount == 0);
    CHECK(driver.accumulatedSeconds() == 0.0);

    // Time spent paused is not replayed as catch-up ticks on resume.
    static_cast<void>(state.apply(LifecycleEvent::resume()));
    CHECK(admitGameCommand(state, command).has_value());
    const FixedStepPlan resumed = planLifecycleTicks(state, driver, 0.05);
    CHECK(resumed.tickCount == 0);
    CHECK(planLifecycleTicks(state, driver, 0.05).tickCount == 1);
}

void backgroundStopsGpuAndRequestsCheckpointOnce() {
    LifecycleState state = runningState();
    const LifecycleEffects entered = state.apply(LifecycleEvent::enterBackground());
    CHECK(entered.checkpointRequested);
    CHECK(entered.gpuSubmissionStopped);
    CHECK(state.suspended());
    CHECK(!state.maySubmitGpu());
    CHECK(!state.advancesSimulation());

    const LifecycleEffects repeated = state.apply(LifecycleEvent::enterBackground());
    CHECK(!repeated.checkpointRequested);

    static_cast<void>(state.apply(LifecycleEvent::enterForeground()));
    CHECK(state.advancesSimulation());
}

void surfaceLossKeepsSimulationState() {
    LifecycleState state = runningState();
    SafeCheckpointLedger ledger;
    ledger.recordTickBoundary(41, snapshotWithHash(0xA1));
    ledger.recordTickBoundary(42, snapshotWithHash(0xB2));
    const auto before = ledger.request();

    const LifecycleEffects lost = state.apply(LifecycleEvent::surfaceLost());
    CHECK(lost.gpuSubmissionStopped);
    CHECK(!state.maySubmitGpu());
    CHECK(!state.advancesSimulation());

    FixedStepDriver driver(10, 5);
    CHECK(planLifecycleTicks(state, driver, 1.0).tickCount == 0);
    // No tick ran, so the authoritative state is the same boundary object.
    CHECK(ledger.request() == before);
    CHECK(ledger.request()->completedTicks == 42);
    CHECK(ledger.request()->canonical.hash == 0xB2);

    const LifecycleEffects regained =
        state.apply(LifecycleEvent::surfaceAvailable(phoneMetrics()));
    CHECK(regained.renderRebuildRequested);
    CHECK(regained.display == phoneMetrics());
    CHECK(state.advancesSimulation());
    ledger.recordTickBoundary(43, snapshotWithHash(0xC3));
    CHECK(ledger.request()->completedTicks == 43);
}

void eventOrderIsApplied() {
    // pause → lost → regained → resume ends running; the reversed tail ends
    // paused. Coalescing would erase the difference.
    LifecycleState ordered = runningState();
    const std::vector<LifecycleEvent> first{
        LifecycleEvent::pause(), LifecycleEvent::surfaceLost(),
        LifecycleEvent::surfaceAvailable(phoneMetrics()), LifecycleEvent::resume()};
    static_cast<void>(ordered.apply(first));
    CHECK(ordered.advancesSimulation());

    LifecycleState reversed = runningState();
    const std::vector<LifecycleEvent> second{
        LifecycleEvent::resume(), LifecycleEvent::surfaceLost(),
        LifecycleEvent::surfaceAvailable(phoneMetrics()), LifecycleEvent::pause()};
    static_cast<void>(reversed.apply(second));
    CHECK(!reversed.advancesSimulation());
}

void batchEffectsMerge() {
    LifecycleState state = runningState();
    DisplayMetrics rotated = phoneMetrics();
    rotated.extentPixels = {2400, 1080};
    rotated.safeArea = {96, 0, 48, 0};
    const std::vector<LifecycleEvent> events{
        LifecycleEvent::memoryPressure(MemoryPressureLevel::Critical),
        LifecycleEvent::displayChanged(rotated),
        LifecycleEvent::memoryPressure(MemoryPressureLevel::Moderate),
        LifecycleEvent::thermalPressure(ThermalLevel::Serious)};
    const LifecycleEffects effects = state.apply(events);
    CHECK(effects.memoryPressure == MemoryPressureLevel::Critical);
    CHECK(effects.thermal == ThermalLevel::Serious);
    CHECK(effects.display == rotated);
    CHECK(effects.renderRebuildRequested);
    // Pressure never stops the simulation rule.
    CHECK(state.advancesSimulation());
}

void displayEventsNeedValidMetrics() {
    LifecycleState state;
    LifecycleEvent missing{.kind = LifecycleEventKind::SurfaceAvailable};
    CHECK_THROWS(std::invalid_argument, static_cast<void>(state.apply(missing)));
    DisplayMetrics empty = phoneMetrics();
    empty.extentPixels = {0, 0};
    CHECK_THROWS(std::invalid_argument,
                 static_cast<void>(state.apply(LifecycleEvent::surfaceAvailable(empty))));
    CHECK(!state.surfaceAvailable());

    // A resize before any surface exists has nothing to rebuild.
    const LifecycleEffects early = state.apply(LifecycleEvent::displayChanged(phoneMetrics()));
    CHECK(!early.renderRebuildRequested);
}

void checkpointLedgerKeepsTickBoundaries() {
    SafeCheckpointLedger ledger;
    CHECK(ledger.request() == nullptr);
    CHECK_THROWS(std::invalid_argument, ledger.recordTickBoundary(1, CanonicalSnapshot{}));

    ledger.recordTickBoundary(1, snapshotWithHash(0x11));
    const auto first = ledger.request();
    ledger.recordTickBoundary(2, snapshotWithHash(0x22));
    // A writer holding the earlier checkpoint keeps an unchanged instance.
    CHECK(first->completedTicks == 1);
    CHECK(first->canonical.hash == 0x11);
    CHECK(ledger.request()->completedTicks == 2);
    CHECK_THROWS(std::logic_error, ledger.recordTickBoundary(2, snapshotWithHash(0x33)));
    CHECK_THROWS(std::logic_error, ledger.recordTickBoundary(1, snapshotWithHash(0x33)));
}

void memoryPressureMapsToStagedEviction() {
    CHECK(geometryEvictionScope(MemoryPressureLevel::Normal) == GeometryEvictionScope::None);
    CHECK(geometryEvictionScope(MemoryPressureLevel::Moderate) == GeometryEvictionScope::Unused);
    CHECK(geometryEvictionScope(MemoryPressureLevel::Critical) ==
          GeometryEvictionScope::AllRegenerable);
}

}  // namespace

int main() {
    queuePreservesPushOrder();
    queueAcceptsEventsFromCallbackThread();
    queueDrainIsOwnerThreadOnly();
    queueOverflowFailsFastOnOwner();
    pauseHoldsTicksAndCommands();
    backgroundStopsGpuAndRequestsCheckpointOnce();
    surfaceLossKeepsSimulationState();
    eventOrderIsApplied();
    batchEffectsMerge();
    displayEventsNeedValidMetrics();
    checkpointLedgerKeepsTickBoundaries();
    memoryPressureMapsToStagedEviction();
    return konbini::test::summarize("konbini_lifecycle_runtime_tests");
}
