#pragma once

#include <array>
#include <cstdint>

#include "konbini/sim/math_types.h"

// @implements spec/interface/visia-presentation.md First playable definitions
// @implements spec/feature/npc-conversations-and-placement-feedback.md Visia dummy

namespace konbini::render {

// Visia is KonbiniDominant's semantic presentation definition. It is not
// Pictor's upstream Visus type; an adapter may lower a Visia to Visus later.
enum class VisiaId : std::uint8_t {
    ResidentPrimitive = 0,
    StoreLandingEffectPrimitive,
    Invalid = 0xFF,
};

enum class VisiaPrimitiveKind : std::uint8_t {
    ResidentBoxes = 0,
    HorizontalAnnulus,
    Invalid = 0xFF,
};

struct VisiaDefinition {
    VisiaId id = VisiaId::Invalid;
    VisiaPrimitiveKind primitiveKind = VisiaPrimitiveKind::Invalid;
};

struct BoxPrimitiveDefinition {
    sim::Vec3 centerMeters{};
    sim::Vec3 halfExtentsMeters{};
    std::array<float, 4> color{1.0F, 1.0F, 1.0F, 1.0F};
};

struct ResidentPrimitiveVisiaDefinition {
    VisiaId id = VisiaId::ResidentPrimitive;
    BoxPrimitiveDefinition body;
    BoxPrimitiveDefinition head;
};

struct StoreLandingEffectPrimitiveVisiaDefinition {
    VisiaId id = VisiaId::StoreLandingEffectPrimitive;
    double startInnerRadiusMeters = 0.0;
    double startOuterRadiusMeters = 0.0;
    double endInnerRadiusMeters = 0.0;
    double endOuterRadiusMeters = 0.0;
    double heightOffsetMeters = 0.0;
    std::uint32_t segmentCount = 0;
    std::array<float, 4> color{1.0F, 1.0F, 1.0F, 1.0F};
};

struct VisiaPose {
    sim::Vec3 positionMeters{};
    double yawDegrees = 0.0;
};

struct VisiaInstance {
    VisiaId id = VisiaId::Invalid;
    VisiaPose pose;
    double normalizedAge = 0.0;
};

// Unknown enum values are rejected instead of resolving to a placeholder.
[[nodiscard]] const VisiaDefinition& visiaDefinition(VisiaId id);
[[nodiscard]] const ResidentPrimitiveVisiaDefinition& residentPrimitiveVisia();
[[nodiscard]] const StoreLandingEffectPrimitiveVisiaDefinition&
storeLandingEffectPrimitiveVisia();

}  // namespace konbini::render
