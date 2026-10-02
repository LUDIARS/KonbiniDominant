#include "konbini/render/speech_bubble_layout.h"

#include <cmath>
#include <stdexcept>

#include "konbini/render/presentation_object_keys.h"
#include "konbini/render/presentation_transform.h"
#include "konbini/render/speech_glyph_atlas.h"
#include "konbini/render/speech_line_catalog.h"

// @implements spec/feature/npc-conversations-and-placement-feedback.md Japanese speech lines

namespace konbini::render {

namespace {

// Noto Sans JP's typographic descender: the em box spans [-0.12, 0.88].
constexpr double kDescenderEm = 0.12;

[[nodiscard]] sim::Vec3 scaled(const sim::Vec3& v, const double s) noexcept {
    return {v.x * s, v.y * s, v.z * s};
}

[[nodiscard]] sim::Vec3 plus(const sim::Vec3& a, const sim::Vec3& b) noexcept {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

[[nodiscard]] bool positiveFinite(const double value) noexcept {
    return std::isfinite(value) && value > 0.0;
}

[[nodiscard]] bool isColor(const WorldColor& color) noexcept {
    for (const float channel : color) {
        if (!std::isfinite(channel) || channel < 0.0F || channel > 1.0F) {
            return false;
        }
    }
    return true;
}

}  // namespace

void validateSpeechBubbleLayoutStyle(const SpeechBubbleLayoutStyle& style) {
    if (!positiveFinite(style.emPixels) ||
        !std::isfinite(style.horizontalPaddingPixels) ||
        style.horizontalPaddingPixels < 0.0 ||
        !std::isfinite(style.verticalPaddingPixels) ||
        style.verticalPaddingPixels < 0.0 ||
        !positiveFinite(style.tailHeightPixels) ||
        !positiveFinite(style.tailHalfWidthPixels) ||
        !positiveFinite(style.textDepthBiasMeters) || style.maxGlyphs == 0 ||
        style.maxGlyphs > 255 || !isColor(style.backgroundColor) ||
        !isColor(style.textColor)) {
        throw std::invalid_argument("invalid speech bubble layout style");
    }
}

// @implements spec/feature/npc-conversations-and-placement-feedback.md Japanese speech lines
void appendSpeechBubbleDraws(
    const IsometricCamera& camera, const VisibleSpeechBubble& bubble,
    const SpeechBubbleLayoutStyle& style, std::vector<PresentationDraw>& out) {
    validateSpeechBubbleLayoutStyle(style);
    if (camera.extent.height == 0 || !positiveFinite(camera.verticalSpanMeters) ||
        !sim::isFinite(bubble.anchorMeters)) {
        throw std::invalid_argument("speech bubble layout needs a valid camera");
    }

    const std::vector<char32_t> codepoints =
        decodeUtf8(speechLine(bubble.lineKey).text);
    if (codepoints.empty() || codepoints.size() > style.maxGlyphs) {
        throw std::invalid_argument(
            "speech line is empty or longer than the bubble glyph limit");
    }
    std::vector<const SpeechGlyph*> glyphs;
    glyphs.reserve(codepoints.size());
    double textWidthEm = 0.0;
    for (const char32_t codepoint : codepoints) {
        const SpeechGlyph& glyph = speechGlyph(codepoint);
        glyphs.push_back(&glyph);
        textWidthEm += glyph.advanceEm;
    }

    const double metersPerPixel =
        camera.verticalSpanMeters / static_cast<double>(camera.extent.height);
    const double em = style.emPixels * metersPerPixel;
    const double textWidth = textWidthEm * em;
    const double width = textWidth + 2.0 * style.horizontalPaddingPixels * metersPerPixel;
    const double height = em + 2.0 * style.verticalPaddingPixels * metersPerPixel;
    const double tailHeight = style.tailHeightPixels * metersPerPixel;

    const sim::Vec3& right = camera.right;
    const sim::Vec3& up = camera.up;
    const sim::Vec3 towardCamera = scaled(camera.forward, -1.0);
    // Bottom center of the background, directly above the tail tip.
    const sim::Vec3 base = plus(bubble.anchorMeters, scaled(up, tailHeight));
    const std::uint64_t owner = residentOwnerKey(bubble.id);
    const std::uint64_t part = bubble.id.ordinal;

    out.push_back({
        .key = {PresentationObjectRole::BubbleBackground, owner, part},
        .mesh = {PresentationMeshKind::BubbleBackground, 0},
        .model = basisModel(scaled(right, width), scaled(up, height),
                            towardCamera, base),
        .tint = style.backgroundColor,
    });
    out.push_back({
        .key = {PresentationObjectRole::BubbleTail, owner, part},
        .mesh = {PresentationMeshKind::BubbleTail, 0},
        .model = basisModel(
            scaled(right, style.tailHalfWidthPixels * metersPerPixel),
            scaled(up, tailHeight), towardCamera, base),
        .tint = style.backgroundColor,
    });

    const double baseline =
        style.verticalPaddingPixels * metersPerPixel + kDescenderEm * em;
    double pen = -0.5 * textWidth;
    for (std::size_t position = 0; position < glyphs.size(); ++position) {
        const SpeechGlyph& glyph = *glyphs[position];
        if (!glyph.indices.empty()) {
            const sim::Vec3 origin = plus(
                plus(base, scaled(right, pen)),
                plus(scaled(up, baseline),
                     scaled(towardCamera, style.textDepthBiasMeters)));
            out.push_back({
                .key = {PresentationObjectRole::SpeechGlyph, owner,
                        speechGlyphPart(bubble.id, position)},
                .mesh = {PresentationMeshKind::SpeechGlyph,
                         static_cast<std::uint32_t>(glyph.codepoint)},
                .model = basisModel(scaled(right, em), scaled(up, em),
                                    scaled(towardCamera, em), origin),
                .tint = style.textColor,
            });
        }
        pen += glyph.advanceEm * em;
    }
}

}  // namespace konbini::render
