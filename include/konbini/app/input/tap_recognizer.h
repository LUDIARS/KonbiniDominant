#pragma once

// @implements spec/interface/mobile-platform.md Input and UI

namespace konbini::app {

// Decides whether one contact sequence ends as a tap. A sequence that was
// disqualified once (drag slop crossed, a second finger landed, the gesture
// was cancelled) never becomes a tap again, even if the finger returns to
// its press position before release.
class TapRecognizer {
public:
    // A first contact went down.
    void begin() noexcept;
    // Drag / pinch / cancel took the sequence. Latched until `begin()`.
    void disqualify() noexcept;
    // The last contact lifted. True only for a sequence that stayed a tap
    // candidate from press to release. Ends the sequence either way.
    [[nodiscard]] bool release() noexcept;
    void reset() noexcept;

    [[nodiscard]] bool tracking() const noexcept { return tracking_; }
    [[nodiscard]] bool candidate() const noexcept { return tracking_ && candidate_; }

private:
    bool tracking_ = false;
    bool candidate_ = false;
};

}  // namespace konbini::app
