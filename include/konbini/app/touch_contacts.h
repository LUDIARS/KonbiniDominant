#pragma once
#include <array>
#include <cstdint>
#include "konbini/app/input/pinch_recognizer.h"
#include "konbini/app/input/touch_sample.h"
#include "konbini/app/pointer_sample.h"
// @implements spec/interface/mobile-platform.md Input and UI
namespace konbini::app {
// Contact table shared by every touch host (Windows WM_TOUCH, Android, iOS).
// Merges per-finger `TouchSample`s into one frame `PointerSample`, keeping
// short press / release edges that land inside one frame.
class TouchContacts {
public:
    static constexpr std::size_t kCapacity=32;
    // Throws `std::invalid_argument` for a non-finite position or a timestamp
    // older than the previous event, `std::runtime_error` when more than
    // `kCapacity` fingers are down. Both cancel the gesture first, so a
    // failing host leaves no stuck contact.
    void update(const TouchSample& sample);
    // Same as above, stamped with the latest timestamp seen.
    void update(std::uint64_t id,TouchPhase phase,double x,double y);
    void cancel() noexcept;
    PointerSample consume() noexcept;
    [[nodiscard]] unsigned liveContacts() const noexcept;
private:
    struct Contact {std::uint64_t id=0;double x=0,y=0;bool used=false;};
    struct Position {double x=0,y=0;ContactPair pair;};
    Position position() const noexcept;
    std::array<Contact,kCapacity> contacts_{};
    PointerSample pending_;
    PinchRecognizer pinch_;
    double lastTimestamp_=0;
    bool multiple_=false;
};
}
