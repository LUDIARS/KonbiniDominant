#include <cstdint>
#include <limits>
#include <stdexcept>

#include "konbini/app/input/drag_recognizer.h"
#include "konbini/app/input/gesture_cancel_recognizer.h"
#include "konbini/app/input/pinch_recognizer.h"
#include "konbini/app/input/tap_recognizer.h"
#include "konbini/app/input/touch_sample.h"
#include "konbini/app/pointer_input_controller.h"
#include "konbini/app/touch_contacts.h"

#include "../../check.h"

// KD-MOB-004 touch normalization and gesture recognizers.
// @implements spec/interface/mobile-platform.md Input and UI
// @implements spec/test/verification-strategy.md 1. Data / ID unit tests

namespace {

using namespace konbini::app;

constexpr konbini::render::ViewportExtent kViewport{1280, 720};

// A layout with one bottom bar and one Place button, no modal.
[[nodiscard]] PointerControls bottomBar() {
    PointerControls controls;
    controls.panel = {0, 600, 1280, 120};
    controls.buttons.push_back({PointerAction::Place, {20, 640, 200, 60}, "PLACE", "", true, false});
    return controls;
}

[[nodiscard]] FrameInput frameWith(const PointerSample& sample) {
    FrameInput input;
    input.pointer = sample;
    input.viewport = kViewport;
    input.dtSeconds = 1.0 / 60.0;
    input.cursorXPixels = sample.xPixels;
    input.cursorYPixels = sample.yPixels;
    input.cursorInsideViewport = sample.xPixels >= 0 && sample.yPixels >= 0 &&
        sample.xPixels < kViewport.width && sample.yPixels < kViewport.height;
    return input;
}

// C-1
void nativeContactKeepsFingerPhasePositionAndTimestamp() {
    const auto sample = normalizeTouchSample(42, TouchPhase::Move, 100.0, 50.0, 3.25, 2.0, 1.5);
    CHECK(sample.fingerId == 42);
    CHECK(sample.phase == TouchPhase::Move);
    CHECK(sample.xPixels == 200.0 && sample.yPixels == 75.0);
    CHECK(sample.timestampSeconds == 3.25);
    CHECK_THROWS(std::invalid_argument,
                 (void)normalizeTouchSample(1, TouchPhase::Down, std::numeric_limits<double>::quiet_NaN(), 0, 0));
    CHECK_THROWS(std::invalid_argument, (void)normalizeTouchSample(1, TouchPhase::Down, 0, 0, -1));
    CHECK_THROWS(std::invalid_argument, (void)normalizeTouchSample(1, TouchPhase::Down, 0, 0, 0, 0.0));
    // Cancel may arrive without a position.
    CHECK_NO_THROW((void)normalizeTouchSample(1, TouchPhase::Cancel,
        std::numeric_limits<double>::quiet_NaN(), 0, 1));

    TouchContacts contacts;
    contacts.update(TouchSample{7, TouchPhase::Down, 10, 20, 1.0});
    contacts.update(TouchSample{7, TouchPhase::Move, 14, 22, 1.5});
    const auto merged = contacts.consume();
    CHECK(merged.fingerId == 7);
    CHECK(merged.pressTimestampSeconds == 1.0 && merged.timestampSeconds == 1.5);
    CHECK(merged.pressXPixels == 10 && merged.xPixels == 14 && merged.yPixels == 22);
    // An older timestamp is rejected and leaves no live contact behind.
    CHECK_THROWS(std::invalid_argument, contacts.update(TouchSample{7, TouchPhase::Move, 15, 22, 1.2}));
    CHECK(contacts.liveContacts() == 0);
}

// C-2
void recognizersOwnOneDecisionEach() {
    TapRecognizer tap;
    tap.begin();
    CHECK(tap.candidate());
    CHECK(tap.release());
    tap.begin();
    tap.disqualify();
    CHECK(!tap.release());
    CHECK(!tap.tracking());

    DragRecognizer drag;
    drag.begin(0, 0, 10);
    PointerSample sample;
    sample.xPixels = 6;
    CHECK(!drag.update(sample).dragging);
    sample.xPixels = 12;
    const auto step = drag.update(sample);
    CHECK(step.dragging && step.deltaXPixels == 6);

    PinchRecognizer pinch;
    pinch.observe({1, 2, 10, 2}, {1, 2, 20, 2});
    pinch.observe({1, 2, 20, 2}, {1, 3, 80, 2});  // pair changed: ignored
    CHECK(pinch.consume() == 2.0);
    CHECK(pinch.consume() == 1.0);

    CHECK(recognizeGestureCancel({}) == GestureCancelReason::None);
    CHECK(recognizeGestureCancel({.contactCancelled = true, .focusLost = true}) ==
          GestureCancelReason::ContactCancelled);
    CHECK(recognizeGestureCancel({.surfaceChanged = true}) == GestureCancelReason::SurfaceChanged);
    CHECK(recognizeGestureCancel({.contextChanged = true}) == GestureCancelReason::ContextChanged);
}

// C-3
void dragStartedContactNeverConfirmsAsTap() {
    TouchContacts contacts;
    PointerInputController controller;
    const auto controls = bottomBar();
    contacts.update(TouchSample{1, TouchPhase::Down, 300, 300, 0.0});
    (void)controller.translate(frameWith(contacts.consume()), controls, 0);
    contacts.update(TouchSample{1, TouchPhase::Move, 340, 300, 0.1});
    const auto dragged = controller.translate(frameWith(contacts.consume()), controls, 0);
    CHECK(dragged.dragDeltaXPixels == 40);
    // Back to the press position, then lift: still not a tap.
    contacts.update(TouchSample{1, TouchPhase::Move, 300, 300, 0.2});
    (void)controller.translate(frameWith(contacts.consume()), controls, 0);
    contacts.update(TouchSample{1, TouchPhase::Up, 300, 300, 0.3});
    const auto released = controller.translate(frameWith(contacts.consume()), controls, 0);
    CHECK(!released.primaryClick);

    // A short contact inside the slop is a touch tap.
    contacts.update(TouchSample{2, TouchPhase::Down, 300, 300, 1.0});
    contacts.update(TouchSample{2, TouchPhase::Up, 304, 302, 1.1});
    const auto tapped = controller.translate(frameWith(contacts.consume()), controls, 0);
    CHECK(tapped.primaryClick && tapped.primaryClickFromTouch);
    CHECK(controller.lastGestureWasTouch());
}

// C-4
void pauseAndCancelLeaveNoStuckPointer() {
    TouchContacts contacts;
    PointerInputController controller;
    const auto controls = bottomBar();
    contacts.update(TouchSample{1, TouchPhase::Down, 300, 300, 0.0});
    (void)controller.translate(frameWith(contacts.consume()), controls, 0);
    CHECK(controller.gestureActive());

    // OS contact cancel: the gesture ends with no tap and no live contact.
    contacts.update(TouchSample{1, TouchPhase::Cancel, 0, 0, 0.1});
    const auto cancelled = controller.translate(frameWith(contacts.consume()), controls, 0);
    CHECK(!cancelled.primaryClick && !controller.gestureActive());
    CHECK(controller.lastCancel() == GestureCancelReason::ContactCancelled);
    CHECK(contacts.liveContacts() == 0);
    // Late Move / Up of the cancelled finger cannot revive it.
    contacts.update(TouchSample{1, TouchPhase::Move, 320, 300, 0.2});
    contacts.update(TouchSample{1, TouchPhase::Up, 320, 300, 0.3});
    const auto late = contacts.consume();
    CHECK(!late.down && !late.pressed && !late.released);
    CHECK(!controller.translate(frameWith(late), controls, 0).primaryClick);

    // App pause (focus loss) with two fingers down drops everything.
    contacts.update(TouchSample{3, TouchPhase::Down, 100, 100, 1.0});
    contacts.update(TouchSample{4, TouchPhase::Down, 200, 100, 1.0});
    (void)controller.translate(frameWith(contacts.consume()), controls, 0);
    contacts.cancel();  // NativeMobileRuntime::pause -> holdSimulation
    auto paused = frameWith(contacts.consume());
    paused.focusLost = true;
    (void)controller.translate(paused, controls, 0);
    CHECK(!controller.gestureActive() && contacts.liveContacts() == 0);
    CHECK(!controller.pressed().has_value());
}

// C-5
void uiCapturedContactNeverReachesTheWorld() {
    TouchContacts contacts;
    PointerInputController controller;
    const auto controls = bottomBar();
    // Press on the bar, drag far into the world, release there.
    contacts.update(TouchSample{1, TouchPhase::Down, 600, 650, 0.0});
    (void)controller.translate(frameWith(contacts.consume()), controls, 0);
    contacts.update(TouchSample{1, TouchPhase::Move, 600, 200, 0.1});
    const auto moved = controller.translate(frameWith(contacts.consume()), controls, 0);
    CHECK(moved.dragDeltaXPixels == 0 && moved.dragDeltaYPixels == 0 && moved.pinchRatio == 1);
    contacts.update(TouchSample{1, TouchPhase::Up, 600, 200, 0.2});
    const auto released = controller.translate(frameWith(contacts.consume()), controls, 0);
    CHECK(!released.primaryClick && !released.buildRequested);

    // Press on Place, slide off, release: no action.
    contacts.update(TouchSample{2, TouchPhase::Down, 100, 670, 1.0});
    (void)controller.translate(frameWith(contacts.consume()), controls, 0);
    contacts.update(TouchSample{2, TouchPhase::Up, 600, 300, 1.1});
    const auto slid = controller.translate(frameWith(contacts.consume()), controls, 0);
    CHECK(!slid.buildRequested && !slid.primaryClick);
}

// C-6 (gesture half): drag pans, pinch zooms.
void dragPansAndPinchZooms() {
    TouchContacts contacts;
    PointerInputController controller;
    const auto controls = bottomBar();
    contacts.update(TouchSample{1, TouchPhase::Down, 300, 300, 0.0});
    contacts.update(TouchSample{2, TouchPhase::Down, 400, 300, 0.0});
    (void)controller.translate(frameWith(contacts.consume()), controls, 0);
    contacts.update(TouchSample{2, TouchPhase::Move, 500, 300, 0.1});
    const auto pinch = controller.translate(frameWith(contacts.consume()), controls, 0);
    CHECK(pinch.pinchRatio == 2.0);
    CHECK(pinch.dragDeltaXPixels == 50.0);
    contacts.update(TouchSample{1, TouchPhase::Up, 300, 300, 0.2});
    contacts.update(TouchSample{2, TouchPhase::Up, 500, 300, 0.2});
    CHECK(!controller.translate(frameWith(contacts.consume()), controls, 0).primaryClick);
}

// C-11: the frame result does not depend on how many raw samples the OS sent.
void sampleCountDoesNotChangeTheOutcome() {
    const auto run = [](int moves) {
        TouchContacts contacts;
        PointerInputController controller;
        const auto controls = bottomBar();
        double t = 0;
        contacts.update(TouchSample{1, TouchPhase::Down, 300, 300, t});
        for (int i = 1; i <= moves; ++i)
            contacts.update(TouchSample{1, TouchPhase::Move, 300 + 60.0 * i / moves, 300, t += 0.001});
        contacts.update(TouchSample{1, TouchPhase::Up, 360, 300, t += 0.001});
        return controller.translate(frameWith(contacts.consume()), controls, 0);
    };
    const auto few = run(1);
    const auto many = run(120);
    CHECK(few.primaryClick == many.primaryClick);
    CHECK(!few.primaryClick);
    CHECK(few.dragDeltaXPixels == many.dragDeltaXPixels);
}

}  // namespace

int main() {
    CHECK_NO_THROW(nativeContactKeepsFingerPhasePositionAndTimestamp());
    CHECK_NO_THROW(recognizersOwnOneDecisionEach());
    CHECK_NO_THROW(dragStartedContactNeverConfirmsAsTap());
    CHECK_NO_THROW(pauseAndCancelLeaveNoStuckPointer());
    CHECK_NO_THROW(uiCapturedContactNeverReachesTheWorld());
    CHECK_NO_THROW(dragPansAndPinchZooms());
    CHECK_NO_THROW(sampleCountDoesNotChangeTheOutcome());
    return konbini::test::summarize("touch gestures");
}
