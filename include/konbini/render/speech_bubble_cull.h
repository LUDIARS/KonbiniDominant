#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include "konbini/render/isometric_camera.h"
#include "konbini/render/resident_pose_tracker.h"

// @implements spec/feature/npc-conversations-and-placement-feedback.md Bubble culling

namespace konbini::render {

struct SpeechBubbleCullSpec {
    // Simultaneous bubble cap (BASE-NPC-BUBBLE-LIMIT-01).
    std::size_t maxVisibleBubbles = 8;
    // Extra NDC margin so a bubble whose anchor just left the screen edge is
    // not popped while its body is still visible.
    double screenMarginNdc = 0.1;
};

void validateSpeechBubbleCullSpec(const SpeechBubbleCullSpec& spec);

struct VisibleSpeechBubble {
    sim::ResidentPresentationId id{};
    // World head anchor: resident position + bubble height.
    sim::Vec3 anchorMeters{};
    double distanceMeters = 0.0;
    std::string lineKey;
};

struct SpeechBubbleCullResult {
    // Nearest first, ties by resident id.
    std::vector<VisibleSpeechBubble> visible;
    std::size_t culledByDistance = 0;
    std::size_t culledOffscreen = 0;
    std::size_t culledByLimit = 0;
};

// Projects each speaking resident's head anchor with the camera's
// view-projection and keeps it only when
// 1. the camera eye is within the resident's `bubble.maxDistanceMeters`,
// 2. the anchor lands inside the viewport (NDC +- margin, depth [0, 1]),
// 3. it is among the `maxVisibleBubbles` nearest survivors.
// An anchor at or below the head top is `std::invalid_argument`.
[[nodiscard]] SpeechBubbleCullResult cullSpeechBubbles(
    const IsometricCamera& camera, std::span<const TrackedResidentPose> poses,
    const SpeechBubbleCullSpec& spec);

}  // namespace konbini::render
