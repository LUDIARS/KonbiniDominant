#include "konbini/render/visia.h"

#include <stdexcept>

// @implements spec/interface/visia-presentation.md First playable definitions
// @implements spec/feature/npc-conversations-and-placement-feedback.md Visia dummy

namespace konbini::render {

namespace {

constexpr VisiaDefinition kResidentDefinition{
    .id = VisiaId::ResidentPrimitive,
    .primitiveKind = VisiaPrimitiveKind::ResidentBoxes,
};

constexpr VisiaDefinition kStoreLandingEffectDefinition{
    .id = VisiaId::StoreLandingEffectPrimitive,
    .primitiveKind = VisiaPrimitiveKind::HorizontalAnnulus,
};

constexpr ResidentPrimitiveVisiaDefinition kResidentPrimitive{
    .id = VisiaId::ResidentPrimitive,
    .body = {
        .centerMeters = {0.0, 0.65, 0.0},
        .halfExtentsMeters = {0.22, 0.65, 0.16},
        .color = {0.18F, 0.48F, 0.82F, 1.0F},
    },
    .head = {
        .centerMeters = {0.0, 1.53, 0.0},
        .halfExtentsMeters = {0.19, 0.19, 0.19},
        .color = {0.96F, 0.75F, 0.58F, 1.0F},
    },
};

constexpr StoreLandingEffectPrimitiveVisiaDefinition kStoreLandingEffect{
    .id = VisiaId::StoreLandingEffectPrimitive,
    .startInnerRadiusMeters = 0.35,
    .startOuterRadiusMeters = 0.65,
    .endInnerRadiusMeters = 2.35,
    .endOuterRadiusMeters = 2.85,
    .heightOffsetMeters = 0.04,
    .segmentCount = 32,
    .color = {1.0F, 0.72F, 0.16F, 0.92F},
};

}  // namespace

// @implements spec/interface/visia-presentation.md First playable definitions
const VisiaDefinition& visiaDefinition(const VisiaId id) {
    switch (id) {
        case VisiaId::ResidentPrimitive:
            return kResidentDefinition;
        case VisiaId::StoreLandingEffectPrimitive:
            return kStoreLandingEffectDefinition;
        case VisiaId::Invalid:
            break;
    }
    throw std::invalid_argument("unknown VisiaId");
}

// @implements spec/feature/npc-conversations-and-placement-feedback.md Visia dummy
const ResidentPrimitiveVisiaDefinition& residentPrimitiveVisia() {
    return kResidentPrimitive;
}

// @implements spec/feature/npc-conversations-and-placement-feedback.md Visia dummy
const StoreLandingEffectPrimitiveVisiaDefinition&
storeLandingEffectPrimitiveVisia() {
    return kStoreLandingEffect;
}

}  // namespace konbini::render
