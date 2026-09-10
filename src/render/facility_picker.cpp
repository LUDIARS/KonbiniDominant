#include "konbini/render/facility_picker.h"
#include "konbini/render/facility_display_bounds.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

// @implements spec/interface/pictor-rendering.md Game-owned render domain

namespace konbini::render {

namespace {

constexpr double kParallelEpsilon = 1.0e-12;
constexpr double kDistanceTieEpsilon = 1.0e-9;

std::optional<double> intersectBounds(
    const WorldRay& ray, const sim::Bounds3& bounds) {
    const std::array<double, 3> origin{
        ray.originMeters.x, ray.originMeters.y, ray.originMeters.z};
    const std::array<double, 3> direction{
        ray.direction.x, ray.direction.y, ray.direction.z};
    const std::array<double, 3> minimum{
        bounds.min.x, bounds.min.y, bounds.min.z};
    const std::array<double, 3> maximum{
        bounds.max.x, bounds.max.y, bounds.max.z};

    double nearDistance = 0.0;
    double farDistance = std::numeric_limits<double>::infinity();
    for (std::size_t axis = 0; axis < origin.size(); ++axis) {
        if (std::abs(direction[axis]) <= kParallelEpsilon) {
            if (origin[axis] < minimum[axis] ||
                origin[axis] > maximum[axis]) {
                return std::nullopt;
            }
            continue;
        }
        double first =
            (minimum[axis] - origin[axis]) / direction[axis];
        double second =
            (maximum[axis] - origin[axis]) / direction[axis];
        if (!std::isfinite(first) || !std::isfinite(second)) {
            throw std::invalid_argument(
                "facility picker ray intersection overflowed");
        }
        if (first > second) {
            std::swap(first, second);
        }
        nearDistance = std::max(nearDistance, first);
        farDistance = std::min(farDistance, second);
        if (nearDistance > farDistance) {
            return std::nullopt;
        }
    }
    return nearDistance;
}

}  // namespace

// 最近接距離を先に確定させてから、その距離の tie 集合を Figmentum key 昇順で
// 解決する 2 pass にする。1 pass で「より近ければ差し替え」にすると、tie の
// 解決結果が snapshot の facility 順に依存して揺れる。
// @implements spec/interface/pictor-rendering.md Game-owned render domain
std::optional<FacilityPick> pickFacility(
    const WorldRay& ray,
    const std::span<const sim::RenderFacility> facilities) {
    const double directionLengthSquared =
        ray.direction.x * ray.direction.x +
        ray.direction.y * ray.direction.y +
        ray.direction.z * ray.direction.z;
    if (!sim::isFinite(ray.originMeters) ||
        !sim::isFinite(ray.direction) ||
        !std::isfinite(directionLengthSquared) ||
        directionLengthSquared <= kParallelEpsilon) {
        throw std::invalid_argument(
            "facility picker requires a finite non-zero world ray");
    }
    const double inverseDirectionLength =
        1.0 / std::sqrt(directionLengthSquared);
    const WorldRay normalizedRay{
        .originMeters = ray.originMeters,
        .direction = {
            ray.direction.x * inverseDirectionLength,
            ray.direction.y * inverseDirectionLength,
            ray.direction.z * inverseDirectionLength,
        },
    };

    std::optional<double> nearestDistance;
    for (const sim::RenderFacility& facility : facilities) {
        if (!facility.id.isValid() ||
            !facility.figmentumKey.isValid() ||
            !sim::isFiniteAndOrdered(facility.boundsMeters)) {
            throw std::invalid_argument(
                "facility picker received an invalid render facility");
        }
        if (facility.state == sim::FacilityState::Destroyed && !facility.isLotRepresentation) {
            continue;
        }
        const std::optional<double> distance =
            intersectBounds(normalizedRay, facilityDisplayBounds(facility));
        if (!distance.has_value()) {
            continue;
        }
        if (!nearestDistance.has_value() ||
            *distance < *nearestDistance) {
            nearestDistance = *distance;
        }
    }
    if (!nearestDistance.has_value()) {
        return std::nullopt;
    }

    std::optional<FacilityPick> closest;
    for (const sim::RenderFacility& facility : facilities) {
        if (facility.state == sim::FacilityState::Destroyed && !facility.isLotRepresentation) {
            continue;
        }
        const std::optional<double> distance =
            intersectBounds(normalizedRay, facilityDisplayBounds(facility));
        if (!distance.has_value() ||
            *distance > *nearestDistance + kDistanceTieEpsilon) {
            continue;
        }
        if (!closest.has_value() ||
            facility.figmentumKey.value() <
                closest->figmentumKey.value()) {
            closest = FacilityPick{
                .figmentumKey = facility.figmentumKey,
                .facilityId = facility.id,
                .distanceMeters = *distance,
            };
        }
    }
    return closest;
}

}  // namespace konbini::render
