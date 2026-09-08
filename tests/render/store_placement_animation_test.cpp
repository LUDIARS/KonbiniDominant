#include "konbini/render/store_placement_animation.h"

#include <cmath>
#include <limits>
#include <stdexcept>

#include "../check.h"

// @implements spec/test/verification-strategy.md 1. Data / ID unit tests
// @implements spec/interface/visia-presentation.md Store placement sampler
// @implements spec/feature/npc-conversations-and-placement-feedback.md Store placement choreography

namespace {

using konbini::render::StorePlacementAnimationSample;
using konbini::render::StorePlacementAnimationSpec;
using konbini::render::StorePlacementEasingCurve;
using konbini::render::VisiaId;
using konbini::render::VisiaPose;
using konbini::render::defaultStorePlacementAnimationSpec;
using konbini::render::sampleStorePlacementAnimation;
using konbini::render::validateStorePlacementAnimationSpec;
using konbini::sim::Vec3;
using konbini::test::approxEqual;

// The cue always targets yaw 0, but a non-zero target here proves the turn is
// measured relative to the final pose instead of to world zero.
const VisiaPose kTargetPose{.positionMeters = {12.0, 0.5, -4.0},
                            .yawDegrees = 30.0};

[[nodiscard]] bool samePose(const VisiaPose& left, const VisiaPose& right) {
    return left.positionMeters.x == right.positionMeters.x &&
           left.positionMeters.y == right.positionMeters.y &&
           left.positionMeters.z == right.positionMeters.z &&
           left.yawDegrees == right.yawDegrees;
}

[[nodiscard]] VisiaPose spawnPoseFor(const StorePlacementAnimationSpec& spec) {
    return {
        .positionMeters = {kTargetPose.positionMeters.x,
                           kTargetPose.positionMeters.y +
                               spec.spawnHeightMeters,
                           kTargetPose.positionMeters.z},
        .yawDegrees = kTargetPose.yawDegrees - spec.rotationDegrees,
    };
}

// --- spec ---------------------------------------------------------------

void testDefaultSpecMatchesTheChoreography() {
    const StorePlacementAnimationSpec spec =
        defaultStorePlacementAnimationSpec();
    CHECK(spec.holdSeconds == 0.2);
    CHECK(spec.fallSeconds == 0.65);
    CHECK(spec.effectSeconds == 0.4);
    CHECK(spec.spawnHeightMeters == 18.0);
    CHECK(spec.rotationDegrees == 270.0);
    CHECK(spec.fallEasing == StorePlacementEasingCurve::EaseInCubic);
    CHECK(spec.landingEffectVisia == VisiaId::StoreLandingEffectPrimitive);
    CHECK_NO_THROW(validateStorePlacementAnimationSpec(spec));
}

void testSpecIsValidated() {
    const StorePlacementAnimationSpec baseline =
        defaultStorePlacementAnimationSpec();

    // A zero hold is legal: the store may start falling immediately.
    StorePlacementAnimationSpec noHold = baseline;
    noHold.holdSeconds = 0.0;
    CHECK_NO_THROW(validateStorePlacementAnimationSpec(noHold));

    StorePlacementAnimationSpec negativeHold = baseline;
    negativeHold.holdSeconds = -0.01;
    CHECK_THROWS(std::invalid_argument,
                 validateStorePlacementAnimationSpec(negativeHold));

    // A zero-length fall or effect would divide by zero when normalised.
    StorePlacementAnimationSpec noFall = baseline;
    noFall.fallSeconds = 0.0;
    CHECK_THROWS(std::invalid_argument,
                 validateStorePlacementAnimationSpec(noFall));

    StorePlacementAnimationSpec noEffect = baseline;
    noEffect.effectSeconds = 0.0;
    CHECK_THROWS(std::invalid_argument,
                 validateStorePlacementAnimationSpec(noEffect));

    StorePlacementAnimationSpec noDrop = baseline;
    noDrop.spawnHeightMeters = 0.0;
    CHECK_THROWS(std::invalid_argument,
                 validateStorePlacementAnimationSpec(noDrop));

    StorePlacementAnimationSpec noTurn = baseline;
    noTurn.rotationDegrees = 0.0;
    CHECK_THROWS(std::invalid_argument,
                 validateStorePlacementAnimationSpec(noTurn));

    StorePlacementAnimationSpec nonFinite = baseline;
    nonFinite.fallSeconds = std::numeric_limits<double>::infinity();
    CHECK_THROWS(std::invalid_argument,
                 validateStorePlacementAnimationSpec(nonFinite));

    // The landing flash has to be the annulus Visia; a resident-shaped Visia
    // would appear as a person standing in the crater.
    StorePlacementAnimationSpec wrongEffect = baseline;
    wrongEffect.landingEffectVisia = VisiaId::ResidentPrimitive;
    CHECK_THROWS(std::invalid_argument,
                 validateStorePlacementAnimationSpec(wrongEffect));

    StorePlacementAnimationSpec invalidEffect = baseline;
    invalidEffect.landingEffectVisia = VisiaId::Invalid;
    CHECK_THROWS(std::invalid_argument,
                 validateStorePlacementAnimationSpec(invalidEffect));
}

// --- hold ---------------------------------------------------------------

void testHoldKeepsTheSpawnPose() {
    const StorePlacementAnimationSpec spec =
        defaultStorePlacementAnimationSpec();
    const VisiaPose spawn = spawnPoseFor(spec);

    for (const double elapsed :
         {0.0, spec.holdSeconds * 0.5,
          std::nextafter(spec.holdSeconds, 0.0)}) {
        const StorePlacementAnimationSample sample =
            sampleStorePlacementAnimation(spec, kTargetPose, elapsed);
        CHECK(samePose(sample.storePose, spawn));
        CHECK(!sample.hasLanded);
        CHECK(!sample.landingEffect.has_value());
        CHECK(!sample.isComplete);
    }

    // The spawn pose is the whole 270-degree turn behind the final yaw and
    // one spawn height above it.
    CHECK(spawn.yawDegrees == kTargetPose.yawDegrees - 270.0);
    CHECK(spawn.positionMeters.y ==
          kTargetPose.positionMeters.y + spec.spawnHeightMeters);
    CHECK(spawn.positionMeters.x == kTargetPose.positionMeters.x);
    CHECK(spawn.positionMeters.z == kTargetPose.positionMeters.z);
}

// --- fall ---------------------------------------------------------------

void testFallStartsAtTheHoldBoundary() {
    const StorePlacementAnimationSpec spec =
        defaultStorePlacementAnimationSpec();
    const VisiaPose spawn = spawnPoseFor(spec);

    // The first falling sample has zero progress, so it still reads exactly
    // as the spawn pose rather than jumping a frame ahead.
    const StorePlacementAnimationSample atHoldEnd =
        sampleStorePlacementAnimation(spec, kTargetPose, spec.holdSeconds);
    CHECK(samePose(atHoldEnd.storePose, spawn));
    CHECK(!atHoldEnd.hasLanded);
    CHECK(!atHoldEnd.landingEffect.has_value());
}

void testFallFollowsTheEasedCurve() {
    const StorePlacementAnimationSpec spec =
        defaultStorePlacementAnimationSpec();
    const VisiaPose spawn = spawnPoseFor(spec);
    constexpr double kTolerance = 1.0e-9;

    const double midpoint = spec.holdSeconds + (spec.fallSeconds * 0.5);
    const StorePlacementAnimationSample sample =
        sampleStorePlacementAnimation(spec, kTargetPose, midpoint);
    const double normalized =
        (midpoint - spec.holdSeconds) / spec.fallSeconds;
    const double eased = normalized * normalized * normalized;

    CHECK(approxEqual(sample.storePose.positionMeters.y,
                      spawn.positionMeters.y +
                          ((kTargetPose.positionMeters.y -
                            spawn.positionMeters.y) *
                           eased),
                      kTolerance));
    CHECK(approxEqual(sample.storePose.yawDegrees,
                      spawn.yawDegrees +
                          ((kTargetPose.yawDegrees - spawn.yawDegrees) *
                           eased),
                      kTolerance));
    // Ease-in means the store is still high up at the halfway point.
    CHECK(sample.storePose.positionMeters.y >
          kTargetPose.positionMeters.y +
              (spec.spawnHeightMeters * 0.5));

    // The drop is monotonic in both height and yaw.
    double previousHeight = std::numeric_limits<double>::infinity();
    double previousYaw = -std::numeric_limits<double>::infinity();
    for (int step = 0; step <= 20; ++step) {
        const double elapsed =
            spec.holdSeconds +
            (spec.fallSeconds * (static_cast<double>(step) / 20.0));
        const StorePlacementAnimationSample stepSample =
            sampleStorePlacementAnimation(spec, kTargetPose, elapsed);
        CHECK(stepSample.storePose.positionMeters.y <= previousHeight);
        CHECK(stepSample.storePose.yawDegrees >= previousYaw);
        previousHeight = stepSample.storePose.positionMeters.y;
        previousYaw = stepSample.storePose.yawDegrees;
        // The horizontal position never moves during the drop.
        CHECK(stepSample.storePose.positionMeters.x ==
              kTargetPose.positionMeters.x);
        CHECK(stepSample.storePose.positionMeters.z ==
              kTargetPose.positionMeters.z);
    }
}

// --- landing ------------------------------------------------------------

// Landing is the first tick at or after hold + fall. Just before it, the
// store is still airborne; at it, the pose is exactly the target and the
// flash starts at age zero.
void testLandingBoundary() {
    const StorePlacementAnimationSpec spec =
        defaultStorePlacementAnimationSpec();
    const double landingTime = spec.holdSeconds + spec.fallSeconds;

    const StorePlacementAnimationSample justBefore =
        sampleStorePlacementAnimation(spec, kTargetPose,
                                      std::nextafter(landingTime, 0.0));
    CHECK(!justBefore.hasLanded);
    CHECK(!justBefore.landingEffect.has_value());
    CHECK(!justBefore.isComplete);

    const StorePlacementAnimationSample atLanding =
        sampleStorePlacementAnimation(spec, kTargetPose, landingTime);
    CHECK(atLanding.hasLanded);
    CHECK(!atLanding.isComplete);
    // The turn ends exactly on the final pose: no residual yaw offset from
    // 270 degrees of rotation.
    CHECK(samePose(atLanding.storePose, kTargetPose));
    CHECK(atLanding.landingEffect.has_value());
    if (atLanding.landingEffect.has_value()) {
        CHECK(atLanding.landingEffect->id ==
              VisiaId::StoreLandingEffectPrimitive);
        CHECK(atLanding.landingEffect->normalizedAge == 0.0);
        CHECK(samePose(atLanding.landingEffect->pose, kTargetPose));
    }
}

// --- landing effect lifetime -------------------------------------------

void testEffectRunsUntilCompletion() {
    const StorePlacementAnimationSpec spec =
        defaultStorePlacementAnimationSpec();
    const double landingTime = spec.holdSeconds + spec.fallSeconds;
    const double completionTime = landingTime + spec.effectSeconds;
    constexpr double kTolerance = 1.0e-9;

    const StorePlacementAnimationSample midEffect =
        sampleStorePlacementAnimation(spec, kTargetPose,
                                      landingTime + (spec.effectSeconds / 2.0));
    CHECK(midEffect.hasLanded);
    CHECK(!midEffect.isComplete);
    CHECK(midEffect.landingEffect.has_value());
    if (midEffect.landingEffect.has_value()) {
        CHECK(approxEqual(midEffect.landingEffect->normalizedAge, 0.5,
                          kTolerance));
    }

    const StorePlacementAnimationSample justBeforeEnd =
        sampleStorePlacementAnimation(spec, kTargetPose,
                                      std::nextafter(completionTime, 0.0));
    CHECK(justBeforeEnd.landingEffect.has_value());
    CHECK(!justBeforeEnd.isComplete);
    if (justBeforeEnd.landingEffect.has_value()) {
        CHECK(justBeforeEnd.landingEffect->normalizedAge < 1.0);
    }

    // At completion the flash is gone; it does not linger at full age.
    const StorePlacementAnimationSample atEnd =
        sampleStorePlacementAnimation(spec, kTargetPose, completionTime);
    CHECK(atEnd.isComplete);
    CHECK(atEnd.hasLanded);
    CHECK(!atEnd.landingEffect.has_value());
    CHECK(samePose(atEnd.storePose, kTargetPose));

    // A finished animation stays finished and keeps the final pose.
    const StorePlacementAnimationSample longAfter =
        sampleStorePlacementAnimation(spec, kTargetPose,
                                      completionTime + 1000.0);
    CHECK(longAfter.isComplete);
    CHECK(longAfter.hasLanded);
    CHECK(!longAfter.landingEffect.has_value());
    CHECK(samePose(longAfter.storePose, kTargetPose));
}

// --- rejected samples ---------------------------------------------------

void testSampleInputIsValidated() {
    const StorePlacementAnimationSpec spec =
        defaultStorePlacementAnimationSpec();

    CHECK_THROWS(std::invalid_argument,
                 (void)sampleStorePlacementAnimation(spec, kTargetPose,
                                                     -0.000001));
    CHECK_THROWS(std::invalid_argument,
                 (void)sampleStorePlacementAnimation(
                     spec, kTargetPose,
                     std::numeric_limits<double>::quiet_NaN()));
    CHECK_THROWS(std::invalid_argument,
                 (void)sampleStorePlacementAnimation(
                     spec, kTargetPose,
                     std::numeric_limits<double>::infinity()));

    const VisiaPose nonFinitePosition{
        .positionMeters = {std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0},
        .yawDegrees = 0.0};
    CHECK_THROWS(std::invalid_argument,
                 (void)sampleStorePlacementAnimation(spec, nonFinitePosition,
                                                     0.5));

    const VisiaPose nonFiniteYaw{
        .positionMeters = {0.0, 0.0, 0.0},
        .yawDegrees = std::numeric_limits<double>::infinity()};
    CHECK_THROWS(std::invalid_argument,
                 (void)sampleStorePlacementAnimation(spec, nonFiniteYaw, 0.5));

    // A spawn offset that cannot be added to the target must fail rather
    // than spawn the store at infinity.
    StorePlacementAnimationSpec hugeDrop = spec;
    hugeDrop.spawnHeightMeters = std::numeric_limits<double>::max();
    CHECK_NO_THROW(validateStorePlacementAnimationSpec(hugeDrop));
    const VisiaPose highTarget{
        .positionMeters = {0.0, std::numeric_limits<double>::max(), 0.0},
        .yawDegrees = 0.0};
    CHECK_THROWS(std::invalid_argument,
                 (void)sampleStorePlacementAnimation(hugeDrop, highTarget,
                                                     0.0));

    StorePlacementAnimationSpec invalidSpec = spec;
    invalidSpec.fallSeconds = -1.0;
    CHECK_THROWS(std::invalid_argument,
                 (void)sampleStorePlacementAnimation(invalidSpec, kTargetPose,
                                                     0.0));
}

// A zero hold has to start the fall on the very first sample, not skip it.
void testZeroHoldStartsFallingImmediately() {
    StorePlacementAnimationSpec spec = defaultStorePlacementAnimationSpec();
    spec.holdSeconds = 0.0;
    const VisiaPose spawn = spawnPoseFor(spec);

    const StorePlacementAnimationSample first =
        sampleStorePlacementAnimation(spec, kTargetPose, 0.0);
    CHECK(samePose(first.storePose, spawn));
    CHECK(!first.hasLanded);

    const StorePlacementAnimationSample landed =
        sampleStorePlacementAnimation(spec, kTargetPose, spec.fallSeconds);
    CHECK(landed.hasLanded);
    CHECK(samePose(landed.storePose, kTargetPose));
}

}  // namespace

int main() {
    testDefaultSpecMatchesTheChoreography();
    testSpecIsValidated();
    testHoldKeepsTheSpawnPose();
    testFallStartsAtTheHoldBoundary();
    testFallFollowsTheEasedCurve();
    testLandingBoundary();
    testEffectRunsUntilCompletion();
    testSampleInputIsValidated();
    testZeroHoldStartsFallingImmediately();
    return konbini::test::summarize("store_placement_animation_test");
}
