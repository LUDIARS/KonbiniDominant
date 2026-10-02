#pragma once

#include <cstddef>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include "konbini/app/platform/lifecycle_event.h"

// @implements spec/interface/mobile-platform.md Lifecycle

namespace konbini::app {

// A lost lifecycle event (a dropped pause or surface loss) leaves the owner
// in a state the OS no longer agrees with, so overflow is a failure, not a
// silent drop.
class LifecycleQueueOverflow : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Bounded hand-off from OS callback threads to the app owner thread.
//
// `push` may be called from any thread and never throws, because OS callback
// threads (JNI, UIKit) are the wrong place to unwind. Overflow is latched and
// reported to the owner by `drain`, which keeps the failure on the thread
// that can stop the app in order.
// @implements spec/interface/mobile-platform.md Lifecycle
class LifecycleEventQueue {
public:
    // `capacity` 0 is rejected (`std::invalid_argument`). The owner is the
    // constructing thread unless `bindOwner` moves it before the loop starts.
    explicit LifecycleEventQueue(std::size_t capacity);

    LifecycleEventQueue(const LifecycleEventQueue&) = delete;
    LifecycleEventQueue& operator=(const LifecycleEventQueue&) = delete;

    void bindOwner(std::thread::id owner) noexcept;

    // Returns false when the event did not fit; the overflow is latched.
    bool push(const LifecycleEvent& event) noexcept;

    // Owner thread only (`std::logic_error` otherwise). Returns events in
    // push order. Throws `LifecycleQueueOverflow` once an event was dropped.
    [[nodiscard]] std::vector<LifecycleEvent> drain();

    [[nodiscard]] std::size_t capacity() const noexcept;
    [[nodiscard]] std::size_t pending() const;

private:
    mutable std::mutex mutex_;
    std::vector<LifecycleEvent> events_;
    std::size_t capacity_ = 0;
    std::size_t droppedEvents_ = 0;
    std::thread::id owner_;
};

}  // namespace konbini::app
