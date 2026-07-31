#pragma once

#include <span>

#include "konbini/render/speech_bubble_geometry.h"
#include "konbini/sim/resident_presentation.h"

// @implements spec/interface/visia-presentation.md Pictor integration boundary
// @implements spec/feature/npc-conversations-and-placement-feedback.md Visia dummy

namespace konbini::render {

struct ResidentVisualGeometry {
    VisiaGeometry residents;
    VisiaGeometry speechBubbles;
};

// Places one ResidentPrimitive Visia at every snapshot resident transform and
// emits camera-facing text only for residents whose snapshot contains speech.
// The returned data is still game-owned CPU geometry; a Pictor adapter owns
// upload, blending, object lifetime, and frame submission.
[[nodiscard]] ResidentVisualGeometry buildResidentVisualGeometry(
    std::span<const sim::ResidentPresentation> residents,
    const IsometricCamera& camera,
    const SpeechBubbleStyle& speechStyle);

}  // namespace konbini::render
