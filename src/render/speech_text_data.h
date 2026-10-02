#pragma once

#include <cstdint>
#include <span>

#include "konbini/render/speech_glyph_atlas.h"

// Raw arrays of src/render/generated/noto_sans_jp_speech_mesh.inc. One
// translation unit includes the generated file so its data exists once; the
// atlas and the catalog read it through these accessors.

namespace konbini::render::detail {

struct SpeechGlyphRange {
    std::uint32_t codepoint;
    std::uint32_t firstPoint;
    std::uint32_t pointCount;
    std::uint32_t firstIndex;
    std::uint32_t indexCount;
    float advanceEm;
};

struct SpeechLineEntry {
    const char* key;
    const char* text;
};

[[nodiscard]] const char* speechDataLocale() noexcept;
[[nodiscard]] std::span<const SpeechGlyphPoint> speechGlyphPoints() noexcept;
[[nodiscard]] std::span<const std::uint16_t> speechGlyphIndices() noexcept;
[[nodiscard]] std::span<const SpeechGlyphRange> speechGlyphRanges() noexcept;
[[nodiscard]] std::span<const SpeechLineEntry> speechLineEntries() noexcept;

}  // namespace konbini::render::detail
