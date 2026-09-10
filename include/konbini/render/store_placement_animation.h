#pragma once

#include <cstdint>
#include <optional>

#include "konbini/render/visia.h"

// @implements spec/interface/visia-presentation.md Store placement sampler
// @implements spec/feature/npc-conversations-and-placement-feedback.md Store placement choreography

namespace konbini::render {

enum class StorePlacementEasingCurve : std::uint8_t {
    EaseInCubic = 0,
};

enum class StorePlacementStage : std::uint8_t { Spin, Hover, Fall, Landed };

struct StorePlacementAnimationSpec {
    double spinSeconds = 0.36;
    double holdSeconds = 0.16;
    double fallSeconds = 0.18;
    double effectSeconds = 0.60;
    double spawnHeightMeters = 8.0;
    double rotationDegrees = 360.0;
    StorePlacementEasingCurve fallEasing =
        StorePlacementEasingCurve::EaseInCubic;
    VisiaId landingEffectVisia =
        VisiaId::StoreLandingEffectPrimitive;
};

struct StorePlacementAnimationSample {
    VisiaPose storePose;
    StorePlacementStage stage = StorePlacementStage::Spin;
    std::optional<VisiaInstance> landingEffect;
    bool hasLanded = false;
    bool isComplete = false;
};

[[nodiscard]] StorePlacementAnimationSpec defaultStorePlacementAnimationSpec()
    noexcept;
void validateStorePlacementAnimationSpec(
    const StorePlacementAnimationSpec& spec);

// elapsedSeconds is presentation time since the accepted placement event.
// A full turn lifts the store, a short hover holds it, then it slams down.
// The final pose is exactly targetPose, including on elevated floors.
[[nodiscard]] StorePlacementAnimationSample sampleStorePlacementAnimation(
    const StorePlacementAnimationSpec& spec,
    const VisiaPose& targetPose,
    double elapsedSeconds);

}  // namespace konbini::render
