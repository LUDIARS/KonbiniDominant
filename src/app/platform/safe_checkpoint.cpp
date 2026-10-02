#include "konbini/app/platform/safe_checkpoint.h"

#include <stdexcept>
#include <utility>

// @implements spec/interface/mobile-platform.md Determinism

namespace konbini::app {

// @implements spec/interface/mobile-platform.md Determinism
void SafeCheckpointLedger::recordTickBoundary(
    const std::uint64_t completedTicks, sim::CanonicalSnapshot canonical) {
    if (canonical.bytes.empty()) {
        throw std::invalid_argument("safe checkpoint needs a canonical snapshot");
    }
    if (latest_ != nullptr && completedTicks <= latest_->completedTicks) {
        throw std::logic_error("safe checkpoint ticks must strictly increase");
    }
    // A fresh object per boundary: a writer still serializing the previous
    // checkpoint keeps its own immutable instance.
    latest_ = std::make_shared<const SafeCheckpoint>(
        SafeCheckpoint{completedTicks, std::move(canonical)});
}

// @implements spec/interface/mobile-platform.md Determinism
std::shared_ptr<const SafeCheckpoint> SafeCheckpointLedger::request() const {
    return latest_;
}

}  // namespace konbini::app
