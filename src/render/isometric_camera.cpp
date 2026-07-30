#include "konbini/render/isometric_camera.h"

#include <cmath>
#include <limits>
#include <stdexcept>

// @implements spec/interface/pictor-rendering.md Game-owned render domain

namespace konbini::render {

namespace {

constexpr double kDegreesToRadians =
    3.141592653589793238462643383279502884 / 180.0;
constexpr double kBasisTolerance = 1.0e-9;

sim::Vec3 add(const sim::Vec3 left, const sim::Vec3 right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

sim::Vec3 subtract(const sim::Vec3 left,
                   const sim::Vec3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

sim::Vec3 scale(const sim::Vec3 value, const double scalar) noexcept {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

double dot(const sim::Vec3 left, const sim::Vec3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

bool isUnit(const sim::Vec3 value) noexcept {
    return std::abs(dot(value, value) - 1.0) <= kBasisTolerance;
}

bool isOrthogonal(
    const sim::Vec3 left, const sim::Vec3 right) noexcept {
    return std::abs(dot(left, right)) <= kBasisTolerance;
}

sim::Vec3 cross(const sim::Vec3 left, const sim::Vec3 right) noexcept {
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

sim::Vec3 normalized(const sim::Vec3 value) {
    const double lengthSquared = dot(value, value);
    if (!std::isfinite(lengthSquared) ||
        lengthSquared <= std::numeric_limits<double>::epsilon()) {
        throw std::invalid_argument("camera basis contains a zero vector");
    }
    return scale(value, 1.0 / std::sqrt(lengthSquared));
}

float checkedFloat(const double value) {
    constexpr double kFloatMax =
        static_cast<double>(std::numeric_limits<float>::max());
    if (!std::isfinite(value) || value < -kFloatMax || value > kFloatMax) {
        throw std::invalid_argument(
            "camera matrix value cannot be represented as float");
    }
    return static_cast<float>(value);
}

std::array<float, 16> makeView(
    const sim::Vec3 eye, const sim::Vec3 forward,
    const sim::Vec3 right, const sim::Vec3 up) {
    return {
        checkedFloat(right.x),
        checkedFloat(up.x),
        checkedFloat(-forward.x),
        0.0F,
        checkedFloat(right.y),
        checkedFloat(up.y),
        checkedFloat(-forward.y),
        0.0F,
        checkedFloat(right.z),
        checkedFloat(up.z),
        checkedFloat(-forward.z),
        0.0F,
        checkedFloat(-dot(right, eye)),
        checkedFloat(-dot(up, eye)),
        checkedFloat(dot(forward, eye)),
        1.0F,
    };
}

// Vulkan clip 空間へ写す orthographic 行列。depth は 0..1 で、Y は
// framebuffer が下向きなので `-2 / height` で反転する。GL 系の -1..1 depth
// 行列をそのまま持ち込むと near 側が clip される。
std::array<float, 16> makeProjection(
    const double width, const double height, const double nearPlane,
    const double farPlane) {
    const double depthSpan = farPlane - nearPlane;
    if (!std::isfinite(width) || width <= 0.0 ||
        !std::isfinite(height) || height <= 0.0 ||
        !std::isfinite(depthSpan) || depthSpan <= 0.0) {
        throw std::invalid_argument(
            "camera projection dimensions are not representable");
    }
    const float horizontalScale = checkedFloat(2.0 / width);
    const float verticalScale = checkedFloat(-2.0 / height);
    const float depthScale = checkedFloat(-1.0 / depthSpan);
    if (horizontalScale == 0.0F || verticalScale == 0.0F ||
        depthScale == 0.0F) {
        throw std::invalid_argument(
            "camera projection scale underflowed");
    }
    std::array<float, 16> result{};
    result[0] = horizontalScale;
    result[5] = verticalScale;
    result[10] = depthScale;
    result[14] = checkedFloat(-nearPlane / depthSpan);
    result[15] = 1.0F;
    return result;
}

// column-major (index = col * 4 + row) の left * right。tick ごとに再計算する
// 値ではないので、determinism を優先して単純な三重ループのままにする。
std::array<float, 16> multiply(
    const std::array<float, 16>& left,
    const std::array<float, 16>& right) noexcept {
    std::array<float, 16> result{};
    for (std::size_t column = 0; column < 4; ++column) {
        for (std::size_t row = 0; row < 4; ++row) {
            float value = 0.0F;
            for (std::size_t inner = 0; inner < 4; ++inner) {
                value += left[inner * 4 + row] *
                         right[column * 4 + inner];
            }
            result[column * 4 + row] = value;
        }
    }
    return result;
}

}  // namespace

// @implements spec/interface/pictor-rendering.md Game-owned render domain
IsometricCamera buildIsometricCamera(
    const IsometricCameraConfig& config, const ViewportExtent extent) {
    if (!sim::isFinite(config.targetMeters) || extent.width == 0 ||
        extent.height == 0 || !std::isfinite(config.distanceMeters) ||
        config.distanceMeters <= 0.0 ||
        !std::isfinite(config.azimuthDegrees) ||
        !std::isfinite(config.elevationDegrees) ||
        config.elevationDegrees <= -89.0 ||
        config.elevationDegrees >= 89.0 ||
        !std::isfinite(config.verticalSpanMeters) ||
        config.verticalSpanMeters <= 0.0 ||
        !std::isfinite(config.nearPlaneMeters) ||
        config.nearPlaneMeters <= 0.0 ||
        !std::isfinite(config.farPlaneMeters) ||
        config.farPlaneMeters <= config.nearPlaneMeters) {
        throw std::invalid_argument("invalid isometric camera configuration");
    }

    const double azimuth = config.azimuthDegrees * kDegreesToRadians;
    const double elevation = config.elevationDegrees * kDegreesToRadians;
    const double horizontalDistance =
        config.distanceMeters * std::cos(elevation);
    const sim::Vec3 eye = add(
        config.targetMeters,
        {horizontalDistance * std::cos(azimuth),
         config.distanceMeters * std::sin(elevation),
         horizontalDistance * std::sin(azimuth)});
    const sim::Vec3 forward =
        normalized(subtract(config.targetMeters, eye));
    const sim::Vec3 right =
        normalized(cross(forward, sim::Vec3{0.0, 1.0, 0.0}));
    const sim::Vec3 up = normalized(cross(right, forward));
    const double aspect =
        static_cast<double>(extent.width) /
        static_cast<double>(extent.height);
    const double horizontalSpan = config.verticalSpanMeters * aspect;

    IsometricCamera camera;
    camera.eyeMeters = eye;
    camera.forward = forward;
    camera.right = right;
    camera.up = up;
    camera.verticalSpanMeters = config.verticalSpanMeters;
    camera.extent = extent;
    camera.view = makeView(eye, forward, right, up);
    camera.projection =
        makeProjection(horizontalSpan, config.verticalSpanMeters,
                       config.nearPlaneMeters, config.farPlaneMeters);
    camera.viewProjection = multiply(camera.projection, camera.view);
    return camera;
}

// camera 側の basis を再検証してから使う。呼び出し側が値型の `IsometricCamera`
// を手で書き換えた場合に、退化した basis のまま ray を作って picker が
// 別 facility を選ぶ、という無言の不整合を防ぐ。
// @implements spec/interface/pictor-rendering.md Game-owned render domain
WorldRay makeWorldRay(
    const IsometricCamera& camera, const double pixelX,
    const double pixelY, const ViewportExtent extent) {
    if (!sim::isFinite(camera.eyeMeters) ||
        !sim::isFinite(camera.forward) ||
        !sim::isFinite(camera.right) ||
        !sim::isFinite(camera.up) ||
        !isUnit(camera.forward) || !isUnit(camera.right) ||
        !isUnit(camera.up) ||
        !isOrthogonal(camera.forward, camera.right) ||
        !isOrthogonal(camera.forward, camera.up) ||
        !isOrthogonal(camera.right, camera.up) ||
        dot(cross(camera.right, camera.forward), camera.up) <
            1.0 - kBasisTolerance ||
        !std::isfinite(camera.verticalSpanMeters) ||
        camera.verticalSpanMeters <= 0.0 ||
        extent.width == 0 || extent.height == 0 ||
        extent != camera.extent ||
        !std::isfinite(pixelX) || !std::isfinite(pixelY) ||
        pixelX < 0.0 || pixelY < 0.0 ||
        pixelX > static_cast<double>(extent.width) ||
        pixelY > static_cast<double>(extent.height)) {
        throw std::invalid_argument(
            "screen point does not match the isometric camera extent");
    }

    const double normalizedX =
        (2.0 * pixelX / static_cast<double>(extent.width)) - 1.0;
    const double normalizedY =
        1.0 - (2.0 * pixelY / static_cast<double>(extent.height));
    const double aspect =
        static_cast<double>(extent.width) /
        static_cast<double>(extent.height);
    const sim::Vec3 horizontal =
        scale(camera.right,
              normalizedX * camera.verticalSpanMeters * aspect * 0.5);
    const sim::Vec3 vertical =
        scale(camera.up,
              normalizedY * camera.verticalSpanMeters * 0.5);
    const sim::Vec3 origin =
        add(add(camera.eyeMeters, horizontal), vertical);
    if (!sim::isFinite(origin)) {
        throw std::invalid_argument(
            "screen point produces a non-finite world ray");
    }
    return {
        .originMeters = origin,
        .direction = camera.forward,
    };
}

}  // namespace konbini::render
