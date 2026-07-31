#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

#include "konbini/render/isometric_camera.h"
#include "konbini/render/visia_geometry.h"

// @implements spec/interface/visia-presentation.md Speech bubble geometry
// @implements spec/feature/npc-conversations-and-placement-feedback.md Conversations

namespace konbini::render {

struct SpeechBubbleRequest {
    sim::Vec3 anchorMeters{};
    std::string_view text;
    double hideDistanceMeters = 0.0;
};

struct SpeechBubbleStyle {
    double pixelSizeMeters = 0.16;
    double glyphSpacingMeters = 0.12;
    double horizontalPaddingMeters = 0.35;
    double verticalPaddingMeters = 0.22;
    double tailHeightMeters = 0.32;
    double tailHalfWidthMeters = 0.25;
    double textDepthBiasMeters = 0.01;
    std::size_t maxCharacters = 24;
    std::array<float, 4> backgroundColor{0.05F, 0.06F, 0.08F, 0.88F};
    std::array<float, 4> textColor{1.0F, 1.0F, 1.0F, 1.0F};
};

// All requests and the style are validated before any geometry is emitted.
// A valid request farther than its hide distance contributes no vertices.
[[nodiscard]] VisiaGeometry buildSpeechBubbleGeometry(
    const IsometricCamera& camera,
    std::span<const SpeechBubbleRequest> requests,
    const SpeechBubbleStyle& style);

}  // namespace konbini::render
