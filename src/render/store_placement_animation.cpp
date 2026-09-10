#include "konbini/render/store_placement_animation.h"

#include <cmath>
#include <numbers>
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
    if (!std::isfinite(spec.spinSeconds) || spec.spinSeconds <= 0.0 ||
        !std::isfinite(spec.holdSeconds) || spec.holdSeconds < 0.0 ||
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
    const double landingTime = spec.spinSeconds + spec.holdSeconds + spec.fallSeconds;
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

    const double landingTime = spec.spinSeconds + spec.holdSeconds + spec.fallSeconds;
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
    const double hoverEnd = spec.spinSeconds + spec.holdSeconds;
    if (elapsedSeconds < spec.spinSeconds) {
        const double t = elapsedSeconds / spec.spinSeconds;
        const double rise = 1.0 - std::pow(1.0 - t, 3.0);
        const double turn = t * t * (3.0 - 2.0 * t);
        sample.storePose = {
            .positionMeters = interpolate(targetPose.positionMeters, spawnPose.positionMeters, rise),
            .yawDegrees = interpolate(spawnPose.yawDegrees, targetPose.yawDegrees, turn),
        };
        sample.stage = StorePlacementStage::Spin;
    } else if (elapsedSeconds < hoverEnd) {
        const double t = (elapsedSeconds - spec.spinSeconds) / spec.holdSeconds;
        sample.storePose = targetPose;
        sample.storePose.positionMeters.y += spec.spawnHeightMeters +
            std::sin(t * std::numbers::pi) * 0.55;
        sample.stage = StorePlacementStage::Hover;
    } else if (elapsedSeconds < landingTime) {
        const double t = (elapsedSeconds - hoverEnd) / spec.fallSeconds;
        sample.storePose = targetPose;
        sample.storePose.positionMeters.y += spec.spawnHeightMeters *
            (1.0 - sampleEasing(spec.fallEasing, t));
        sample.stage = StorePlacementStage::Fall;
    } else {
        sample.storePose = targetPose;
        sample.stage = StorePlacementStage::Landed;
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
