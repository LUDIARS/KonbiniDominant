#include "konbini/render/visia_geometry.h"

#include <cstddef>
#include <cstdint>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "visia_geometry_support.h"

// @implements spec/interface/visia-presentation.md CPU primitive geometry
// @implements spec/feature/npc-conversations-and-placement-feedback.md Visia dummy

namespace konbini::render {

namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;

[[nodiscard]] sim::Vec3 rotateYaw(const sim::Vec3 value,
                                  const double cosine,
                                  const double sine) noexcept {
    return {
        cosine * value.x + sine * value.z,
        value.y,
        -sine * value.x + cosine * value.z,
    };
}

void validateBox(const BoxPrimitiveDefinition& box) {
    if (!sim::isFinite(box.centerMeters) ||
        !sim::isFinite(box.halfExtentsMeters) ||
        box.halfExtentsMeters.x <= 0.0 ||
        box.halfExtentsMeters.y <= 0.0 ||
        box.halfExtentsMeters.z <= 0.0 ||
        !detail::isValidColor(box.color)) {
        throw std::invalid_argument("invalid resident box definition");
    }
}

void validateResidentDefinition(
    const ResidentPrimitiveVisiaDefinition& definition) {
    if (definition.id != VisiaId::ResidentPrimitive) {
        throw std::invalid_argument(
            "resident geometry requires ResidentPrimitive Visia");
    }
    validateBox(definition.body);
    validateBox(definition.head);
    const double bodyBottom =
        definition.body.centerMeters.y - definition.body.halfExtentsMeters.y;
    const double bodyTop =
        definition.body.centerMeters.y + definition.body.halfExtentsMeters.y;
    const double headBottom =
        definition.head.centerMeters.y - definition.head.halfExtentsMeters.y;
    if (bodyBottom < 0.0 || headBottom < bodyTop) {
        throw std::invalid_argument(
            "resident boxes must stand above the feet without overlap");
    }
}

void appendBoxFace(VisiaGeometry& geometry,
                   const BoxPrimitiveDefinition& box,
                   const VisiaPose& pose,
                   const double cosine,
                   const double sine,
                   const sim::Vec3 faceOffset,
                   const sim::Vec3 horizontal,
                   const sim::Vec3 vertical,
                   const sim::Vec3 normal) {
    if (geometry.vertices.size() >
        static_cast<std::size_t>(
            std::numeric_limits<std::uint32_t>::max() - 4U)) {
        throw std::overflow_error("resident geometry vertex index overflow");
    }
    const std::uint32_t base =
        static_cast<std::uint32_t>(geometry.vertices.size());
    const sim::Vec3 faceCenter = detail::add(box.centerMeters, faceOffset);
    const sim::Vec3 localPositions[4] = {
        detail::add(detail::add(faceCenter, detail::scale(horizontal, -1.0)),
                    detail::scale(vertical, -1.0)),
        detail::add(detail::add(faceCenter, horizontal),
                    detail::scale(vertical, -1.0)),
        detail::add(detail::add(faceCenter, horizontal), vertical),
        detail::add(detail::add(faceCenter, detail::scale(horizontal, -1.0)),
                    vertical),
    };
    const sim::Vec3 worldNormal = rotateYaw(normal, cosine, sine);
    for (const sim::Vec3 localPosition : localPositions) {
        detail::appendVertex(
            geometry,
            detail::add(rotateYaw(localPosition, cosine, sine),
                        pose.positionMeters),
            worldNormal,
            box.color);
    }
    geometry.indices.insert(
        geometry.indices.end(),
        {base, base + 1U, base + 2U, base, base + 2U, base + 3U});
}

void appendBox(VisiaGeometry& geometry,
               const BoxPrimitiveDefinition& box,
               const VisiaPose& pose,
               const double cosine,
               const double sine) {
    const double x = box.halfExtentsMeters.x;
    const double y = box.halfExtentsMeters.y;
    const double z = box.halfExtentsMeters.z;

    appendBoxFace(geometry, box, pose, cosine, sine,
                  {x, 0.0, 0.0}, {0.0, 0.0, -z}, {0.0, y, 0.0},
                  {1.0, 0.0, 0.0});
    appendBoxFace(geometry, box, pose, cosine, sine,
                  {-x, 0.0, 0.0}, {0.0, 0.0, z}, {0.0, y, 0.0},
                  {-1.0, 0.0, 0.0});
    appendBoxFace(geometry, box, pose, cosine, sine,
                  {0.0, y, 0.0}, {x, 0.0, 0.0}, {0.0, 0.0, -z},
                  {0.0, 1.0, 0.0});
    appendBoxFace(geometry, box, pose, cosine, sine,
                  {0.0, -y, 0.0}, {x, 0.0, 0.0}, {0.0, 0.0, z},
                  {0.0, -1.0, 0.0});
    appendBoxFace(geometry, box, pose, cosine, sine,
                  {0.0, 0.0, z}, {x, 0.0, 0.0}, {0.0, y, 0.0},
                  {0.0, 0.0, 1.0});
    appendBoxFace(geometry, box, pose, cosine, sine,
                  {0.0, 0.0, -z}, {-x, 0.0, 0.0}, {0.0, y, 0.0},
                  {0.0, 0.0, -1.0});
}

}  // namespace

// @implements spec/interface/visia-presentation.md CPU primitive geometry
// @implements spec/feature/npc-conversations-and-placement-feedback.md Visia dummy
VisiaGeometry buildResidentPrimitiveGeometry(
    const ResidentPrimitiveVisiaDefinition& definition,
    const VisiaPose& pose) {
    validateResidentDefinition(definition);
    if (!sim::isFinite(pose.positionMeters) ||
        !std::isfinite(pose.yawDegrees)) {
        throw std::invalid_argument("invalid resident Visia pose");
    }

    const double radians = pose.yawDegrees * kPi / 180.0;
    const double cosine = std::cos(radians);
    const double sine = std::sin(radians);
    VisiaGeometry geometry;
    geometry.vertices.reserve(48);
    geometry.indices.reserve(72);
    appendBox(geometry, definition.body, pose, cosine, sine);
    appendBox(geometry, definition.head, pose, cosine, sine);
    return geometry;
}

}  // namespace konbini::render
