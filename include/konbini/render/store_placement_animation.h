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

struct StorePlacementAnimationSpec {
    double holdSeconds = 0.2;
    double fallSeconds = 0.65;
    double effectSeconds = 0.4;
    double spawnHeightMeters = 18.0;
    double rotationDegrees = 270.0;
    StorePlacementEasingCurve fallEasing =
        StorePlacementEasingCurve::EaseInCubic;
    VisiaId landingEffectVisia =
        VisiaId::StoreLandingEffectPrimitive;
};

struct StorePlacementAnimationSample {
    VisiaPose storePose;
    std::optional<VisiaInstance> landingEffect;
    bool hasLanded = false;
    bool isComplete = false;
};

[[nodiscard]] StorePlacementAnimationSpec defaultStorePlacementAnimationSpec()
    noexcept;
void validateStorePlacementAnimationSpec(
    const StorePlacementAnimationSpec& spec);

// elapsedSeconds is presentation time since the accepted placement event.
// The returned final store pose is exactly targetPose; the 270-degree turn
// therefore starts behind that final yaw rather than leaving a yaw offset.
[[nodiscard]] StorePlacementAnimationSample sampleStorePlacementAnimation(
    const StorePlacementAnimationSpec& spec,
    const VisiaPose& targetPose,
    double elapsedSeconds);

}  // namespace konbini::render
