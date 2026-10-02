#pragma once

#include <cstdint>

// @implements spec/interface/mobile-platform.md Input and UI

namespace konbini::app {

// The first two live contacts in contact-table order.
struct ContactPair {
    std::uint64_t first = 0;
    std::uint64_t second = 0;
    double distancePixels = 0.0;
    unsigned count = 0;
};

// Accumulates the zoom scale between two contact-table states. Only motion
// of the same two fingers counts: a finger landing or lifting changes the
// pair and must not read as a zoom jump.
class PinchRecognizer {
public:
    void observe(const ContactPair& before, const ContactPair& after) noexcept;
    // Scale since the last consume (1 = no pinch), then restarts at 1.
    [[nodiscard]] double consume() noexcept;
    void reset() noexcept;

private:
    double ratio_ = 1.0;
};

}  // namespace konbini::app
