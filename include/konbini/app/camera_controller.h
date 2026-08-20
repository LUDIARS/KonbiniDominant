#pragma once

#include "konbini/app/frame_input.h"
#include "konbini/render/isometric_camera.h"
#include "konbini/sim/math_types.h"

// @implements spec/plan/tasks/first-playable.md Minimal controls
// @implements spec/interface/pictor-rendering.md Game-owned render domain

namespace konbini::app {

// camera 操作の tuning 値。gameplay へは影響しない presentation 定数なので、
// canonical state にも save にも入らない。
struct CameraControlSpec {
    double panMetersPerSecond = 60.0;
    // drag は「掴んだ地面が指に付いてくる」感覚に合わせ、pixel を viewport
    // 高さに対する world span の比で meter へ直す。
    double dragMetersPerPixel = 1.0;
    double zoomStepRatio = 1.12;
    double minVerticalSpanMeters = 20.0;
    double maxVerticalSpanMeters = 400.0;
    // target が都市の外へ出続けると何も映らなくなるため、生成された都市の
    // bounds を余白付きで clamp 範囲にする。
    double targetMarginMeters = 40.0;
};

// `IsometricCameraConfig` の可変部分 (target / vertical span) だけを操作する
// controller。行列生成は `buildIsometricCamera()` の責務で、ここでは持たない。
class CameraController {
public:
    CameraController(
        render::IsometricCameraConfig initialConfig, sim::Bounds3 cityBounds,
        CameraControlSpec spec);

    void apply(const FrameInput& input);

    [[nodiscard]] const render::IsometricCameraConfig& config()
        const noexcept;
    [[nodiscard]] const CameraControlSpec& spec() const noexcept;

private:
    void clampTarget();

    render::IsometricCameraConfig config_;
    sim::Bounds3 cityBounds_{};
    CameraControlSpec spec_;
};

}  // namespace konbini::app
