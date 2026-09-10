// @implements spec/feature/grid-town-and-vector-ui.md Vector font
#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace konbini::render {
using VectorFontPoint = std::array<std::int16_t, 2>;
struct VectorGlyph {
    std::span<const VectorFontPoint> points;
    std::span<const std::uint16_t> indices;
    float advance;
};
inline constexpr float kVectorFontUnits = 4096.0F;
[[nodiscard]] VectorGlyph vectorGlyph(char character);
void validateVectorFontText(std::string_view text);
}
