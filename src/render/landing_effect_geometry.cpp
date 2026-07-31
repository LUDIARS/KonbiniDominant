#include "konbini/render/visia_geometry.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <stdexcept>

#include "visia_geometry_support.h"

// @implements spec/interface/visia-presentation.md CPU primitive geometry
// @implements spec/feature/npc-conversations-and-placement-feedback.md Visia dummy

namespace konbini::render {

namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr std::uint32_t kMaximumAnnulusSegments = 4096;

void validateLandingDefinition(
    const StoreLandingEffectPrimitiveVisiaDefinition& definition) {
    if (definition.id != VisiaId::StoreLandingEffectPrimitive ||
        !std::isfinite(definition.startInnerRadiusMeters) ||
        !std::isfinite(definition.startOuterRadiusMeters) ||
        !std::isfinite(definition.endInnerRadiusMeters) ||
        !std::isfinite(definition.endOuterRadiusMeters) ||
        !std::isfinite(definition.heightOffsetMeters) ||
        definition.startInnerRadiusMeters <= 0.0 ||
        definition.startOuterRadiusMeters <=
            definition.startInnerRadiusMeters ||
        definition.endInnerRadiusMeters <
            definition.startInnerRadiusMeters ||
        definition.endOuterRadiusMeters <
            definition.startOuterRadiusMeters ||
        definition.endOuterRadiusMeters <= definition.endInnerRadiusMeters ||
        definition.heightOffsetMeters < 0.0 ||
        definition.segmentCount < 3 ||
        definition.segmentCount > kMaximumAnnulusSegments ||
        !detail::isValidColor(definition.color)) {
        throw std::invalid_argument(
            "invalid store landing effect Visia definition");
    }
}

[[nodiscard]] double easeOutCubic(const double value) noexcept {
    const double inverse = 1.0 - value;
    return 1.0 - inverse * inverse * inverse;
}

[[nodiscard]] double interpolate(const double start,
                                 const double end,
                                 const double amount) noexcept {
    return start + (end - start) * amount;
}

}  // namespace

// @implements spec/interface/visia-presentation.md CPU primitive geometry
// @implements spec/feature/npc-conversations-and-placement-feedback.md Visia dummy
VisiaGeometry buildStoreLandingEffectPrimitiveGeometry(
    const StoreLandingEffectPrimitiveVisiaDefinition& definition,
    const sim::Vec3& originMeters,
    const double normalizedAge) {
    validateLandingDefinition(definition);
    if (!sim::isFinite(originMeters) || !std::isfinite(normalizedAge) ||
        normalizedAge < 0.0 || normalizedAge > 1.0) {
        throw std::invalid_argument("invalid store landing effect sample");
    }

    const double radiusProgress = easeOutCubic(normalizedAge);
    const double innerRadius = interpolate(
        definition.startInnerRadiusMeters,
        definition.endInnerRadiusMeters,
        radiusProgress);
    const double outerRadius = interpolate(
        definition.startOuterRadiusMeters,
        definition.endOuterRadiusMeters,
        radiusProgress);
    const double fade = 1.0 - normalizedAge;
    std::array<float, 4> color = definition.color;
    color[3] = detail::checkedFloat(
        static_cast<double>(definition.color[3]) * fade * fade);

    VisiaGeometry geometry;
    const std::size_t pairCount =
        static_cast<std::size_t>(definition.segmentCount) + 1U;
    geometry.vertices.reserve(pairCount * 2U);
    geometry.indices.reserve(
        static_cast<std::size_t>(definition.segmentCount) * 6U);
    const double y = originMeters.y + definition.heightOffsetMeters;
    for (std::uint32_t segment = 0;
         segment <= definition.segmentCount;
         ++segment) {
        const double angle =
            2.0 * kPi * static_cast<double>(segment) /
            static_cast<double>(definition.segmentCount);
        const double cosine = std::cos(angle);
        const double sine = std::sin(angle);
        detail::appendVertex(
            geometry,
            {originMeters.x + cosine * innerRadius,
             y,
             originMeters.z + sine * innerRadius},
            {0.0, 1.0, 0.0},
            color);
        detail::appendVertex(
            geometry,
            {originMeters.x + cosine * outerRadius,
             y,
             originMeters.z + sine * outerRadius},
            {0.0, 1.0, 0.0},
            color);
    }
    for (std::uint32_t segment = 0;
         segment < definition.segmentCount;
         ++segment) {
        const std::uint32_t inner = segment * 2U;
        const std::uint32_t outer = inner + 1U;
        const std::uint32_t nextInner = inner + 2U;
        const std::uint32_t nextOuter = inner + 3U;
        geometry.indices.insert(
            geometry.indices.end(),
            {inner, nextInner, nextOuter,
             inner, nextOuter, outer});
    }
    return geometry;
}

}  // namespace konbini::render
