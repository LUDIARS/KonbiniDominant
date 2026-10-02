#include <cstdint>

#include "ergo/input/mouse_device.h"
#include "konbini/adapters/ergo/touch_pointer_injection.h"
#include "konbini/app/touch_contacts.h"

#include "../check.h"

// KD-MOB-004 Ergo input injection adapter: the primary touch contact as
// Ergo pointer state, never left pressed after release or cancel.
// @implements spec/interface/mobile-platform.md Input and UI

namespace {

using konbini::adapters::ergo::planTouchPointerInjection;
using konbini::app::TouchContacts;
using konbini::app::TouchPhase;
using konbini::app::TouchSample;

constexpr auto bit(::ergo::input::MouseButton button) {
    return static_cast<std::uint8_t>(1U << static_cast<std::uint8_t>(button));
}

void contactHoldsLeftButtonOnlyWhileDown() {
    const auto left = bit(::ergo::input::MouseButton::Left);
    const auto right = bit(::ergo::input::MouseButton::Right);
    TouchContacts contacts;
    contacts.update(TouchSample{5, TouchPhase::Down, 30, 40, 0.0});
    const auto down = planTouchPointerInjection(contacts.consume(), 0);
    CHECK(down.xPixels == 30.0F && down.yPixels == 40.0F);
    CHECK(down.buttons == left);

    contacts.update(TouchSample{5, TouchPhase::Up, 32, 40, 0.1});
    CHECK(planTouchPointerInjection(contacts.consume(), 0).buttons == 0);

    contacts.update(TouchSample{6, TouchPhase::Down, 30, 40, 0.2});
    (void)contacts.consume();
    contacts.update(TouchSample{6, TouchPhase::Cancel, 0, 0, 0.3});
    CHECK(planTouchPointerInjection(contacts.consume(), 0).buttons == 0);

    // A held mouse button is not released by a touch frame.
    contacts.update(TouchSample{7, TouchPhase::Down, 1, 1, 0.4});
    contacts.update(TouchSample{7, TouchPhase::Up, 1, 1, 0.5});
    CHECK(planTouchPointerInjection(contacts.consume(), right).buttons == right);
}

}  // namespace

int main() {
    CHECK_NO_THROW(contactHoldsLeftButtonOnlyWhileDown());
    return konbini::test::summarize("touch pointer injection");
}
