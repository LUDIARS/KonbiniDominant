#include "konbini/render/store_placement_animation.h"
#include <cmath>
#include <limits>
#include <stdexcept>
#include "../check.h"
// @implements spec/feature/store-construction-effects.md Choreography
namespace {
using namespace konbini::render;
using konbini::test::approxEqual;
const VisiaPose target{{12.0, 19.2, -4.0}, 30.0};
bool samePose(const VisiaPose& a, const VisiaPose& b) {
    return a.positionMeters.x==b.positionMeters.x && a.positionMeters.y==b.positionMeters.y &&
           a.positionMeters.z==b.positionMeters.z && a.yawDegrees==b.yawDegrees;
}
void motionAndImpact() {
    const auto spec = defaultStorePlacementAnimationSpec();
    const auto at = [&](double t) { return sampleStorePlacementAnimation(spec,target,t); };
    const auto start = at(0);
    CHECK(start.stage==StorePlacementStage::Spin);
    CHECK(start.storePose.positionMeters.y==target.positionMeters.y);
    CHECK(start.storePose.yawDegrees==target.yawDegrees-360);
    double previousY=target.positionMeters.y, previousYaw=start.storePose.yawDegrees;
    for(unsigned i=1;i<=20;++i) {
        const auto frame=at(spec.spinSeconds*i/20.0);
        CHECK(frame.storePose.positionMeters.y>=previousY);
        CHECK(frame.storePose.yawDegrees>=previousYaw);
        CHECK(!frame.hasLanded && !frame.landingEffect);
        previousY=frame.storePose.positionMeters.y;previousYaw=frame.storePose.yawDegrees;
    }
    const auto hover=at(spec.spinSeconds);
    CHECK(hover.stage==StorePlacementStage::Hover);
    CHECK(hover.storePose.yawDegrees==target.yawDegrees);
    CHECK(hover.storePose.positionMeters.y==target.positionMeters.y+spec.spawnHeightMeters);
    CHECK(at(spec.spinSeconds+spec.holdSeconds/2).storePose.positionMeters.y >
          hover.storePose.positionMeters.y);
    const double fallStart=spec.spinSeconds+spec.holdSeconds;
    const double landing=fallStart+spec.fallSeconds;
    previousY=hover.storePose.positionMeters.y;
    for(unsigned i=0;i<=20;++i) {
        const auto frame=at(fallStart+spec.fallSeconds*i/20.0);
        CHECK(frame.storePose.positionMeters.y<=previousY);
        CHECK(frame.storePose.yawDegrees==target.yawDegrees);
        CHECK(frame.storePose.positionMeters.x==target.positionMeters.x);
        CHECK(frame.storePose.positionMeters.z==target.positionMeters.z);
        previousY=frame.storePose.positionMeters.y;
    }
    CHECK(!at(std::nextafter(landing,0.0)).hasLanded);
    const auto impact=at(landing);
    CHECK(impact.stage==StorePlacementStage::Landed && impact.hasLanded);
    CHECK(samePose(impact.storePose,target));
    CHECK(impact.landingEffect && impact.landingEffect->normalizedAge==0);
    const auto dust=at(landing+spec.effectSeconds/2);
    CHECK(dust.landingEffect && approxEqual(dust.landingEffect->normalizedAge,0.5,1e-9));
    CHECK(!at(std::nextafter(landing+spec.effectSeconds,0.0)).isComplete);
    const auto end=at(landing+spec.effectSeconds);
    CHECK(end.isComplete && !end.landingEffect && samePose(end.storePose,target));
    CHECK(samePose(at(1000).storePose,target));
}
void invalidInputsAndOptionalHover() {
    const auto good=defaultStorePlacementAnimationSpec();
    CHECK_NO_THROW(validateStorePlacementAnimationSpec(good));
    for(const auto field : {&StorePlacementAnimationSpec::spinSeconds,
                            &StorePlacementAnimationSpec::fallSeconds,
                            &StorePlacementAnimationSpec::effectSeconds,
                            &StorePlacementAnimationSpec::spawnHeightMeters,
                            &StorePlacementAnimationSpec::rotationDegrees}) {
        auto bad=good;bad.*field=0;
        CHECK_THROWS(std::invalid_argument,validateStorePlacementAnimationSpec(bad));
        bad.*field=std::numeric_limits<double>::infinity();
        CHECK_THROWS(std::invalid_argument,validateStorePlacementAnimationSpec(bad));
    }
    auto bad=good;bad.holdSeconds=-1;
    CHECK_THROWS(std::invalid_argument,validateStorePlacementAnimationSpec(bad));
    bad=good;bad.landingEffectVisia=VisiaId::ResidentPrimitive;
    CHECK_THROWS(std::invalid_argument,validateStorePlacementAnimationSpec(bad));
    bad=good;bad.landingEffectVisia=VisiaId::Invalid;
    CHECK_THROWS(std::invalid_argument,validateStorePlacementAnimationSpec(bad));
    bad=good;bad.fallEasing=static_cast<StorePlacementEasingCurve>(255);
    CHECK_THROWS(std::invalid_argument,validateStorePlacementAnimationSpec(bad));
    for(double t:{-0.1,std::numeric_limits<double>::infinity(),
                  std::numeric_limits<double>::quiet_NaN()})
        CHECK_THROWS(std::invalid_argument,(void)sampleStorePlacementAnimation(good,target,t));
    auto invalidTarget=target;invalidTarget.yawDegrees=std::numeric_limits<double>::infinity();
    CHECK_THROWS(std::invalid_argument,(void)sampleStorePlacementAnimation(good,invalidTarget,0));
    invalidTarget=target;invalidTarget.positionMeters.x=std::numeric_limits<double>::quiet_NaN();
    CHECK_THROWS(std::invalid_argument,(void)sampleStorePlacementAnimation(good,invalidTarget,0));
    bad=good;bad.spawnHeightMeters=std::numeric_limits<double>::max();
    invalidTarget=target;invalidTarget.positionMeters.y=std::numeric_limits<double>::max();
    CHECK_THROWS(std::invalid_argument,(void)sampleStorePlacementAnimation(bad,invalidTarget,0));
    auto noHover=good;noHover.holdSeconds=0;
    CHECK_NO_THROW(validateStorePlacementAnimationSpec(noHover));
    CHECK(sampleStorePlacementAnimation(noHover,target,noHover.spinSeconds).stage==StorePlacementStage::Fall);
    CHECK(sampleStorePlacementAnimation(noHover,target,noHover.spinSeconds+noHover.fallSeconds).hasLanded);
}
}
int main() {
    motionAndImpact();
    invalidInputsAndOptionalHover();
    return konbini::test::summarize("store construction choreography");
}
