#pragma once

#include <cstddef>
#include <vector>

#include "konbini/render/isometric_camera.h"
#include "konbini/render/presentation_draw.h"
#include "konbini/render/speech_bubble_cull.h"

// @implements spec/feature/npc-conversations-and-placement-feedback.md Japanese speech lines
// @implements spec/interface/visia-presentation.md Speech bubble geometry

namespace konbini::render {

// Bubble dimensions are in screen pixels and converted with the camera's
// meters-per-pixel, so a bubble keeps its on-screen size across zoom.
struct SpeechBubbleLayoutStyle {
    double emPixels = 18.0;
    double horizontalPaddingPixels = 8.0;
    double verticalPaddingPixels = 5.0;
    double tailHeightPixels = 9.0;
    double tailHalfWidthPixels = 6.0;
    // Glyphs sit this far in front of the background along the view axis.
    double textDepthBiasMeters = 0.02;
    // At most 255 so a glyph position fits the object key's low byte.
    std::size_t maxGlyphs = 16;
    WorldColor backgroundColor{0.05F, 0.06F, 0.08F, 1.0F};
    WorldColor textColor{1.0F, 1.0F, 1.0F, 1.0F};
};

void validateSpeechBubbleLayoutStyle(const SpeechBubbleLayoutStyle& style);

// Lays out one visible bubble on the camera-facing plane through its head
// anchor: tail tip at the anchor, background above it, the localized line
// centered on the background. Draws reference the shared bubble / glyph
// meshes; whitespace advances the pen without a draw. The line is resolved by
// its localization key (`speechLine`), and a glyph missing from the atlas or
// a line longer than `maxGlyphs` is an exception, never a truncated bubble.
void appendSpeechBubbleDraws(
    const IsometricCamera& camera, const VisibleSpeechBubble& bubble,
    const SpeechBubbleLayoutStyle& style, std::vector<PresentationDraw>& out);

}  // namespace konbini::render
