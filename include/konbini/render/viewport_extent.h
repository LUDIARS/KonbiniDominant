#pragma once

#include <cstdint>

namespace konbini::render {

struct ViewportExtent {
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    friend bool operator==(
        const ViewportExtent&, const ViewportExtent&) = default;
};

}  // namespace konbini::render
