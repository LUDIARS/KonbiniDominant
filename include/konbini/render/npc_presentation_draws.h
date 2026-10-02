#pragma once

#include <span>
#include <vector>

#include "konbini/render/isometric_camera.h"
#include "konbini/render/presentation_draw.h"
#include "konbini/render/resident_pose_tracker.h"
#include "konbini/render/speech_bubble_cull.h"
#include "konbini/render/speech_bubble_layout.h"
#include "konbini/render/store_construction_visual.h"

// @implements spec/feature/npc-conversations-and-placement-feedback.md Runtime resident sync
// @implements spec/interface/visia-presentation.md Pictor integration boundary

namespace konbini::render {

struct NpcPresentationSpec {
    SpeechBubbleCullSpec bubbleCull;
    SpeechBubbleLayoutStyle bubbleLayout;
};

struct NpcPresentationFrame {
    std::vector<PresentationDraw> draws;
    SpeechBubbleCullResult bubbles;
};

// One frame of resident / landing-effect / bubble objects:
// - one Resident draw per tracked pose, all on the shared ResidentBody mesh
// - one LandingEffect draw per construction visual whose placement sample
//   currently carries a landing effect (translucent, baked age frame)
// - bubble background, tail and glyph draws for the culled bubbles
// Order: residents, effects, bubbles (nearest first). Keys are unique.
[[nodiscard]] NpcPresentationFrame buildNpcPresentationDraws(
    std::span<const TrackedResidentPose> residents,
    std::span<const StoreConstructionVisual> construction,
    const IsometricCamera& camera, const NpcPresentationSpec& spec);

}  // namespace konbini::render
