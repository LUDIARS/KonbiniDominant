#include "konbini/render/store_placement_animation.h"

#include <cmath>
#include <stdexcept>

// @implements spec/interface/visia-presentation.md Store placement sampler
// @implements spec/feature/npc-conversations-and-placement-feedback.md Store placement choreography

namespace konbini::render {

namespace {

[[nodiscard]] double sampleEasing(
    const StorePlacementEasingCurve curve,
    const double normalizedTime) {
    switch (curve) {
        case StorePlacementEasingCurve::EaseInCubic:
            return normalizedTime * normalizedTime * normalizedTime;
    }
    throw std::invalid_argument("unknown store placement easing curve");
}

[[nodiscard]] double interpolate(const double start,
                                 const double end,
                                 const double amount) noexcept {
    return start + (end - start) * amount;
}

[[nodiscard]] sim::Vec3 interpolate(const sim::Vec3 start,
                                    const sim::Vec3 end,
                                    const double amount) noexcept {
    return {
        interpolate(start.x, end.x, amount),
        interpolate(start.y, end.y, amount),
        interpolate(start.z, end.z, amount),
    };
}

}  // namespace

// @implements spec/interface/visia-presentation.md Store placement sampler
StorePlacementAnimationSpec defaultStorePlacementAnimationSpec() noexcept {
    return {};
}

// @implements spec/interface/visia-presentation.md Store placement sampler
void validateStorePlacementAnimationSpec(
    const StorePlacementAnimationSpec& spec) {
    if (!std::isfinite(spec.holdSeconds) || spec.holdSeconds < 0.0 ||
        !std::isfinite(spec.fallSeconds) || spec.fallSeconds <= 0.0 ||
        !std::isfinite(spec.effectSeconds) || spec.effectSeconds <= 0.0 ||
        !std::isfinite(spec.spawnHeightMeters) ||
        spec.spawnHeightMeters <= 0.0 ||
        !std::isfinite(spec.rotationDegrees) ||
        spec.rotationDegrees == 0.0 ||
        spec.landingEffectVisia !=
            VisiaId::StoreLandingEffectPrimitive) {
        throw std::invalid_argument("invalid store placement animation spec");
    }
    (void)sampleEasing(spec.fallEasing, 0.0);
    const VisiaDefinition& effect =
        visiaDefinition(spec.landingEffectVisia);
    if (effect.primitiveKind != VisiaPrimitiveKind::HorizontalAnnulus) {
        throw std::invalid_argument(
            "store placement landing Visia must be a horizontal annulus");
    }
    const double landingTime = spec.holdSeconds + spec.fallSeconds;
    if (!std::isfinite(landingTime) ||
        !std::isfinite(landingTime + spec.effectSeconds)) {
        throw std::invalid_argument(
            "store placement animation duration overflow");
    }
}

// @implements spec/feature/npc-conversations-and-placement-feedback.md Store placement choreography
StorePlacementAnimationSample sampleStorePlacementAnimation(
    const StorePlacementAnimationSpec& spec,
    const VisiaPose& targetPose,
    const double elapsedSeconds) {
    validateStorePlacementAnimationSpec(spec);
    if (!sim::isFinite(targetPose.positionMeters) ||
        !std::isfinite(targetPose.yawDegrees) ||
        !std::isfinite(elapsedSeconds) || elapsedSeconds < 0.0) {
        throw std::invalid_argument("invalid store placement animation sample");
    }

    const double landingTime = spec.holdSeconds + spec.fallSeconds;
    const double completionTime = landingTime + spec.effectSeconds;
    const VisiaPose spawnPose{
        .positionMeters = {
            targetPose.positionMeters.x,
            targetPose.positionMeters.y + spec.spawnHeightMeters,
            targetPose.positionMeters.z,
        },
        .yawDegrees = targetPose.yawDegrees - spec.rotationDegrees,
    };
    if (!sim::isFinite(spawnPose.positionMeters) ||
        !std::isfinite(spawnPose.yawDegrees)) {
        throw std::invalid_argument(
            "store placement spawn pose is not representable");
    }

    StorePlacementAnimationSample sample;
    if (elapsedSeconds < spec.holdSeconds) {
        sample.storePose = spawnPose;
    } else if (elapsedSeconds < landingTime) {
        const double normalizedFall =
            (elapsedSeconds - spec.holdSeconds) / spec.fallSeconds;
        const double eased = sampleEasing(spec.fallEasing, normalizedFall);
        sample.storePose = {
            .positionMeters = interpolate(
                spawnPose.positionMeters,
                targetPose.positionMeters,
                eased),
            .yawDegrees = interpolate(
                spawnPose.yawDegrees,
                targetPose.yawDegrees,
                eased),
        };
    } else {
        sample.storePose = targetPose;
        sample.hasLanded = true;
    }

    if (elapsedSeconds >= landingTime &&
        elapsedSeconds < completionTime) {
        sample.landingEffect = VisiaInstance{
            .id = spec.landingEffectVisia,
            .pose = targetPose,
            .normalizedAge =
                (elapsedSeconds - landingTime) / spec.effectSeconds,
        };
    }
    sample.isComplete = elapsedSeconds >= completionTime;
    return sample;
}

}  // namespace konbini::render
