#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "konbini/render/world_mesh.h"

// @implements spec/feature/npc-conversations-and-placement-feedback.md Japanese speech lines
// @implements spec/interface/visia-presentation.md Speech bubble geometry

namespace konbini::render {

// Glyph outline point in the em square: x = 4096 per em, y = 4096 at the
// baseline, decreasing upward (the same convention as `VectorFontPoint`).
using SpeechGlyphPoint = std::array<std::int16_t, 2>;
inline constexpr float kSpeechGlyphUnitsPerEm = 4096.0F;

// One glyph of the shared speech atlas: the Noto Sans JP subset baked by
// tools/bake_speech_glyphs.py for exactly the code points of the localized
// speech catalog. Whitespace has an advance but no triangles.
struct SpeechGlyph {
    char32_t codepoint = 0;
    std::span<const SpeechGlyphPoint> points;
    std::span<const std::uint16_t> indices;
    float advanceEm = 0.0F;
};

// Every baked glyph in ascending code point order.
[[nodiscard]] std::span<const SpeechGlyph> speechGlyphs() noexcept;

// A code point outside the baked subset is `std::out_of_range`; there is no
// fallback glyph.
[[nodiscard]] const SpeechGlyph& speechGlyph(char32_t codepoint);

// Strict UTF-8 decode. Malformed, overlong or surrogate sequences are
// `std::invalid_argument`.
[[nodiscard]] std::vector<char32_t> decodeUtf8(std::string_view text);

// The glyph as a mesh in its local em space: baseline at y = 0, origin at the
// pen position, z = 0, zero normals (unshaded), one colour. A glyph without
// triangles is `std::invalid_argument`; callers skip it by its empty indices.
[[nodiscard]] WorldMesh buildSpeechGlyphMesh(
    const SpeechGlyph& glyph, const WorldVertex::ColorRgba& color);

}  // namespace konbini::render
