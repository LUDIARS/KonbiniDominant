#include "konbini/app/platform/lifecycle_event_queue.h"

#include <string>
#include <utility>

// @implements spec/interface/mobile-platform.md Lifecycle

namespace konbini::app {

LifecycleEventQueue::LifecycleEventQueue(const std::size_t capacity)
    : capacity_(capacity), owner_(std::this_thread::get_id()) {
    if (capacity == 0) {
        throw std::invalid_argument("lifecycle event queue capacity must be positive");
    }
    // Reserve up front so `push` does not allocate on an OS callback thread.
    events_.reserve(capacity);
}

void LifecycleEventQueue::bindOwner(const std::thread::id owner) noexcept {
    std::scoped_lock lock(mutex_);
    owner_ = owner;
}

// @implements spec/interface/mobile-platform.md Lifecycle
bool LifecycleEventQueue::push(const LifecycleEvent& event) noexcept {
    std::scoped_lock lock(mutex_);
    if (droppedEvents_ != 0 || events_.size() >= capacity_) {
        // Once one event is lost the order is broken; later events are
        // dropped too so the owner never applies a sequence with a hole.
        ++droppedEvents_;
        return false;
    }
    events_.push_back(event);
    return true;
}

// @implements spec/interface/mobile-platform.md Lifecycle
// @implements spec/interface/mobile-platform.md Failure policy
std::vector<LifecycleEvent> LifecycleEventQueue::drain() {
    std::scoped_lock lock(mutex_);
    if (std::this_thread::get_id() != owner_) {
        throw std::logic_error("lifecycle events must be drained on the app owner thread");
    }
    if (droppedEvents_ != 0) {
        throw LifecycleQueueOverflow(
            "lifecycle event queue overflowed; dropped " +
            std::to_string(droppedEvents_) + " event(s)");
    }
    std::vector<LifecycleEvent> drained;
    drained.reserve(capacity_);
    drained.swap(events_);
    return drained;
}

std::size_t LifecycleEventQueue::capacity() const noexcept {
    return capacity_;
}

std::size_t LifecycleEventQueue::pending() const {
    std::scoped_lock lock(mutex_);
    return events_.size();
}

}  // namespace konbini::app
