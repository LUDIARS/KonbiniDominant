#pragma once

#include <cstdint>
#include <memory>

#include "konbini/sim/canonical_snapshot.h"

// @implements spec/interface/mobile-platform.md Determinism

namespace konbini::app {

// One completed tick's canonical state. Immutable once published, so a save
// writer can serialize it off the owner thread.
struct SafeCheckpoint {
    std::uint64_t completedTicks = 0;
    sim::CanonicalSnapshot canonical;
};

// Keeps the latest tick-boundary state for checkpoint requests.
//
// Requests are answered from the last completed tick, never from a state
// mid-tick: a suspend that arrives between ticks gets the boundary before
// it, and the lifecycle gate stops further ticks until resume.
// @implements spec/interface/mobile-platform.md Determinism
class SafeCheckpointLedger {
public:
    // Owner thread, right after a tick completes. Ticks must strictly
    // increase (`std::logic_error`) and the snapshot must not be empty
    // (`std::invalid_argument`).
    void recordTickBoundary(
        std::uint64_t completedTicks, sim::CanonicalSnapshot canonical);

    // Null until the first tick completes: there is nothing to persist that
    // the content and seed cannot rebuild.
    [[nodiscard]] std::shared_ptr<const SafeCheckpoint> request() const;

private:
    std::shared_ptr<const SafeCheckpoint> latest_;
};

}  // namespace konbini::app
