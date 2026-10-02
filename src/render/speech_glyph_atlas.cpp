#include "konbini/render/speech_glyph_atlas.h"

#include <algorithm>
#include <stdexcept>

#include "speech_text_data.h"

// @implements spec/feature/npc-conversations-and-placement-feedback.md Japanese speech lines

namespace konbini::render {

namespace {

[[nodiscard]] std::vector<SpeechGlyph> buildGlyphTable() {
    const auto points = detail::speechGlyphPoints();
    const auto indices = detail::speechGlyphIndices();
    std::vector<SpeechGlyph> glyphs;
    glyphs.reserve(detail::speechGlyphRanges().size());
    for (const detail::SpeechGlyphRange& range : detail::speechGlyphRanges()) {
        if (static_cast<std::size_t>(range.firstPoint) + range.pointCount >
                points.size() ||
            static_cast<std::size_t>(range.firstIndex) + range.indexCount >
                indices.size() ||
            range.indexCount % 3U != 0U ||
            (!glyphs.empty() && glyphs.back().codepoint >= range.codepoint)) {
            throw std::logic_error("baked speech glyph table is corrupt");
        }
        glyphs.push_back({
            .codepoint = static_cast<char32_t>(range.codepoint),
            .points = points.subspan(range.firstPoint, range.pointCount),
            .indices = indices.subspan(range.firstIndex, range.indexCount),
            .advanceEm = range.advanceEm,
        });
    }
    return glyphs;
}

[[nodiscard]] const std::vector<SpeechGlyph>& glyphTable() {
    static const std::vector<SpeechGlyph> table = buildGlyphTable();
    return table;
}

}  // namespace

std::span<const SpeechGlyph> speechGlyphs() noexcept {
    return glyphTable();
}

const SpeechGlyph& speechGlyph(const char32_t codepoint) {
    const std::vector<SpeechGlyph>& table = glyphTable();
    const auto found = std::lower_bound(
        table.begin(), table.end(), codepoint,
        [](const SpeechGlyph& glyph, const char32_t value) {
            return glyph.codepoint < value;
        });
    if (found == table.end() || found->codepoint != codepoint) {
        throw std::out_of_range(
            "speech glyph atlas has no glyph for this code point");
    }
    return *found;
}

// @implements spec/feature/npc-conversations-and-placement-feedback.md Japanese speech lines
std::vector<char32_t> decodeUtf8(const std::string_view text) {
    std::vector<char32_t> output;
    output.reserve(text.size());
    std::size_t position = 0;
    while (position < text.size()) {
        const auto lead = static_cast<unsigned char>(text[position]);
        std::size_t length = 0;
        char32_t value = 0;
        char32_t minimum = 0;
        if (lead < 0x80U) {
            length = 1;
            value = lead;
        } else if ((lead & 0xE0U) == 0xC0U) {
            length = 2;
            value = lead & 0x1FU;
            minimum = 0x80;
        } else if ((lead & 0xF0U) == 0xE0U) {
            length = 3;
            value = lead & 0x0FU;
            minimum = 0x800;
        } else if ((lead & 0xF8U) == 0xF0U) {
            length = 4;
            value = lead & 0x07U;
            minimum = 0x10000;
        } else {
            throw std::invalid_argument("speech text has an invalid UTF-8 lead");
        }
        if (position + length > text.size()) {
            throw std::invalid_argument("speech text has a truncated UTF-8 sequence");
        }
        for (std::size_t offset = 1; offset < length; ++offset) {
            const auto next =
                static_cast<unsigned char>(text[position + offset]);
            if ((next & 0xC0U) != 0x80U) {
                throw std::invalid_argument(
                    "speech text has an invalid UTF-8 continuation");
            }
            value = (value << 6U) | (next & 0x3FU);
        }
        if (value < minimum || value > 0x10FFFF ||
            (value >= 0xD800 && value <= 0xDFFF)) {
            throw std::invalid_argument(
                "speech text has an overlong or invalid UTF-8 code point");
        }
        output.push_back(value);
        position += length;
    }
    return output;
}

WorldMesh buildSpeechGlyphMesh(
    const SpeechGlyph& glyph, const WorldVertex::ColorRgba& color) {
    if (glyph.indices.empty() || glyph.points.empty()) {
        throw std::invalid_argument("speech glyph has no triangles");
    }
    WorldMesh mesh;
    mesh.vertices.reserve(glyph.points.size());
    for (const SpeechGlyphPoint& point : glyph.points) {
        mesh.vertices.push_back(WorldVertex{
            .position = {
                static_cast<float>(point[0]) / kSpeechGlyphUnitsPerEm,
                (kSpeechGlyphUnitsPerEm - static_cast<float>(point[1])) /
                    kSpeechGlyphUnitsPerEm,
                0.0F,
            },
            .normal = {0.0F, 0.0F, 0.0F},
            .color = color,
        });
    }
    mesh.indices.reserve(glyph.indices.size());
    for (const std::uint16_t index : glyph.indices) {
        if (index >= glyph.points.size()) {
            throw std::logic_error("speech glyph index is outside its points");
        }
        mesh.indices.push_back(index);
    }
    return mesh;
}

}  // namespace konbini::render
