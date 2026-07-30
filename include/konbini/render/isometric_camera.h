#pragma once

#include <array>

#include "konbini/render/viewport_extent.h"
#include "konbini/render/world_ray.h"
#include "konbini/sim/math_types.h"

// @implements spec/interface/pictor-rendering.md Game-owned render domain

namespace konbini::render {

// azimuth / elevation は右手系 Y-up の meter 空間で解釈する。elevation の
// 既定値は真の isometric 角 (atan(1/sqrt(2)) = 35.264...°)。
struct IsometricCameraConfig {
    sim::Vec3 targetMeters{};
    double distanceMeters = 180.0;
    double azimuthDegrees = 45.0;
    double elevationDegrees = 35.264389682754654;
    double verticalSpanMeters = 160.0;
    double nearPlaneMeters = 0.1;
    double farPlaneMeters = 1000.0;
};

// `view` / `projection` / `viewProjection` は column-major (index = col * 4 +
// row)、右手系 view 空間、Vulkan clip 空間 (depth 0..1、Y 下向き) の
// orthographic 行列。この規約は GPU 側 shader と共有するので変更しない。
struct IsometricCamera {
    sim::Vec3 eyeMeters{};
    sim::Vec3 forward{};
    sim::Vec3 right{};
    sim::Vec3 up{};
    double verticalSpanMeters = 0.0;
    ViewportExtent extent{};
    std::array<float, 16> view{};
    std::array<float, 16> projection{};
    std::array<float, 16> viewProjection{};
};

// 不正な設定 (非有限値、extent 0、near >= far 等) は `std::invalid_argument`。
// 無言 clamp すると camera と picker の extent がずれるため落とさない。
[[nodiscard]] IsometricCamera buildIsometricCamera(
    const IsometricCameraConfig& config, ViewportExtent extent);

// orthographic なので direction は常に `camera.forward` で、pixel 位置は
// eye 平面上の origin だけを動かす。`extent` は camera 構築時と一致必須で、
// pixel は [0, width] / [0, height] の閉区間。範囲外は
// `std::invalid_argument` (呼び出し側で viewport 内へ clamp する)。
[[nodiscard]] WorldRay makeWorldRay(
    const IsometricCamera& camera, double pixelX, double pixelY,
    ViewportExtent extent);

}  // namespace konbini::render
