#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

// @implements spec/interface/visia-presentation.md Speech bubble geometry
// @implements spec/feature/npc-conversations-and-placement-feedback.md Conversations

namespace konbini::render {

inline constexpr std::uint32_t kBitmapGlyphWidth = 5;
inline constexpr std::uint32_t kBitmapGlyphHeight = 7;

using BitmapGlyphRows = std::array<std::uint8_t, kBitmapGlyphHeight>;

// Only ASCII A-Z and space are supported. Unsupported input is an error;
// callers must not silently replace it with a missing-glyph box.
[[nodiscard]] const BitmapGlyphRows& bitmapGlyph5x7(char character);
void validateBitmapFontText(std::string_view text);

}  // namespace konbini::render
