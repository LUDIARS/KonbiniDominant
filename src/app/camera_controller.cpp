#include "konbini/app/camera_controller.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

// @implements spec/plan/tasks/first-playable.md Minimal controls

namespace konbini::app {
namespace {

void validateSpec(const CameraControlSpec& spec) {
    if (!std::isfinite(spec.panMetersPerSecond) ||
        spec.panMetersPerSecond <= 0.0 ||
        !std::isfinite(spec.dragMetersPerPixel) ||
        spec.dragMetersPerPixel < 0.0 ||
        !std::isfinite(spec.zoomStepRatio) || spec.zoomStepRatio <= 1.0 ||
        !std::isfinite(spec.minVerticalSpanMeters) ||
        spec.minVerticalSpanMeters <= 0.0 ||
        !std::isfinite(spec.maxVerticalSpanMeters) ||
        spec.maxVerticalSpanMeters <= spec.minVerticalSpanMeters ||
        !std::isfinite(spec.targetMarginMeters) ||
        spec.targetMarginMeters < 0.0) {
        throw std::invalid_argument("camera control spec is invalid");
    }
}

}  // namespace

CameraController::CameraController(
    render::IsometricCameraConfig initialConfig, const sim::Bounds3 cityBounds,
    CameraControlSpec spec)
    : config_(initialConfig), cityBounds_(cityBounds), spec_(spec) {
    validateSpec(spec_);
    if (!sim::isFiniteAndOrdered(cityBounds_)) {
        throw std::invalid_argument(
            "camera controller requires ordered city bounds");
    }
    if (!std::isfinite(config_.verticalSpanMeters) ||
        config_.verticalSpanMeters <= 0.0 ||
        !sim::isFinite(config_.targetMeters)) {
        throw std::invalid_argument(
            "camera controller requires a finite initial configuration");
    }
    config_.verticalSpanMeters = std::clamp(
        config_.verticalSpanMeters, spec_.minVerticalSpanMeters,
        spec_.maxVerticalSpanMeters);
    clampTarget();
}

// @implements spec/plan/tasks/first-playable.md Minimal controls
void CameraController::apply(const FrameInput& input) {
    if (input.focusLost) {
        // focus を失ったフレームの残り入力は捨てる。押しっぱなし判定のまま
        // camera が流れ続けるのを避ける。
        return;
    }
    if (!std::isfinite(input.dtSeconds) || input.dtSeconds < 0.0) {
        throw std::invalid_argument(
            "camera controller requires a finite non-negative delta");
    }

    if(!std::isfinite(input.pinchRatio) || input.pinchRatio<=0)
        throw std::invalid_argument("invalid pinch ratio");
    config_.verticalSpanMeters=std::clamp(config_.verticalSpanMeters/input.pinchRatio,
        spec_.minVerticalSpanMeters,spec_.maxVerticalSpanMeters);
    // zoom は乗算なので、方向キーの pan より先に反映して drag/pan の
    // meter 換算に新しい span を使う。
    if (input.zoomSteps != 0.0) {
        if (!std::isfinite(input.zoomSteps)) {
            throw std::invalid_argument("zoom input must be finite");
        }
        const double factor = std::pow(spec_.zoomStepRatio, -input.zoomSteps);
        config_.verticalSpanMeters = std::clamp(
            config_.verticalSpanMeters * factor, spec_.minVerticalSpanMeters,
            spec_.maxVerticalSpanMeters);
    }

    if (!std::isfinite(input.panRight) || !std::isfinite(input.panForward) ||
        !std::isfinite(input.dragDeltaXPixels) ||
        !std::isfinite(input.dragDeltaYPixels)) {
        throw std::invalid_argument("camera pan input must be finite");
    }

    // azimuth は Y-up 右手系。screen right / screen forward を XZ 平面へ
    // 落とした基底で target を動かす。
    const double azimuthRadians =
        config_.azimuthDegrees * std::numbers::pi / 180.0;
    const double sinAzimuth = std::sin(azimuthRadians);
    const double cosAzimuth = std::cos(azimuthRadians);

    double rightMeters = input.panRight * spec_.panMetersPerSecond *
                         input.dtSeconds;
    double forwardMeters = input.panForward * spec_.panMetersPerSecond *
                           input.dtSeconds;

    if (input.viewport.height != 0 &&
        (input.dragDeltaXPixels != 0.0 || input.dragDeltaYPixels != 0.0)) {
        // drag は「掴んだ地面が指に付いてくる」向き。pixel は viewport 高さに
        // 対する world span 比で meter へ直すので、zoom しても感覚が揃う。
        const double metersPerPixel =
            config_.verticalSpanMeters /
            static_cast<double>(input.viewport.height) *
            spec_.dragMetersPerPixel;
        rightMeters -= input.dragDeltaXPixels * metersPerPixel;
        forwardMeters += input.dragDeltaYPixels * metersPerPixel;
    }

    config_.targetMeters.x += rightMeters * cosAzimuth -
                              forwardMeters * sinAzimuth;
    config_.targetMeters.z += rightMeters * sinAzimuth +
                              forwardMeters * cosAzimuth;
    clampTarget();
}

const render::IsometricCameraConfig& CameraController::config()
    const noexcept {
    return config_;
}

const CameraControlSpec& CameraController::spec() const noexcept {
    return spec_;
}

void CameraController::focusHeight(const double meters) {
    if (!std::isfinite(meters) || meters < 0 || meters > 3000) { throw std::invalid_argument("invalid floor height"); }
    config_.targetMeters.y = meters;
    config_.farPlaneMeters = std::max(config_.farPlaneMeters, config_.distanceMeters * 4.0 + meters + 100.0);
}
void CameraController::clampTarget() {
    config_.targetMeters.x = std::clamp(
        config_.targetMeters.x, cityBounds_.min.x - spec_.targetMarginMeters,
        cityBounds_.max.x + spec_.targetMarginMeters);
    config_.targetMeters.z = std::clamp(
        config_.targetMeters.z, cityBounds_.min.z - spec_.targetMarginMeters,
        cityBounds_.max.z + spec_.targetMarginMeters);
    config_.targetMeters.y = std::clamp(
        config_.targetMeters.y, cityBounds_.min.y, cityBounds_.max.y);
}

}  // namespace konbini::app
