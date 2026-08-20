#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

// @implements spec/interface/visia-presentation.md Speech bubble geometry
// @implements spec/feature/npc-conversations-and-placement-feedback.md Conversations
// @implements spec/feature/ui-ux.md Common HUD

namespace konbini::render {

inline constexpr std::uint32_t kBitmapGlyphWidth = 5;
inline constexpr std::uint32_t kBitmapGlyphHeight = 7;

using BitmapGlyphRows = std::array<std::uint8_t, kBitmapGlyphHeight>;

// Supported repertoire: ASCII A-Z, 0-9, space and the punctuation used by the
// HUD (`-`, `:`, `/`, `.`, `+`). Digits exist because the HUD prints cash,
// store count and tick numbers; without them the model would have to spell
// numbers out. Unsupported input is an error; callers must not silently
// replace it with a missing-glyph box.
[[nodiscard]] const BitmapGlyphRows& bitmapGlyph5x7(char character);
[[nodiscard]] bool isBitmapFontCharacter(char character) noexcept;
void validateBitmapFontText(std::string_view text);

}  // namespace konbini::render
