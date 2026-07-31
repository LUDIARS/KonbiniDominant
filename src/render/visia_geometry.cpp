#include "konbini/render/visia_geometry.h"

#include <cmath>
#include <stdexcept>

// @implements spec/interface/visia-presentation.md CPU primitive geometry
// @implements spec/feature/npc-conversations-and-placement-feedback.md Visia dummy

namespace konbini::render {

// @implements spec/interface/visia-presentation.md CPU primitive geometry
// @implements spec/interface/visia-presentation.md First playable definitions
VisiaGeometry buildPrimitiveVisiaGeometry(const VisiaInstance& instance) {
    if (!sim::isFinite(instance.pose.positionMeters) ||
        !std::isfinite(instance.pose.yawDegrees) ||
        !std::isfinite(instance.normalizedAge)) {
        throw std::invalid_argument("invalid Visia instance");
    }

    const VisiaDefinition& definition = visiaDefinition(instance.id);
    switch (definition.primitiveKind) {
        case VisiaPrimitiveKind::ResidentBoxes:
            if (instance.normalizedAge != 0.0) {
                throw std::invalid_argument(
                    "resident Visia does not accept normalized age");
            }
            return buildResidentPrimitiveGeometry(
                residentPrimitiveVisia(), instance.pose);
        case VisiaPrimitiveKind::HorizontalAnnulus:
            return buildStoreLandingEffectPrimitiveGeometry(
                storeLandingEffectPrimitiveVisia(),
                instance.pose.positionMeters,
                instance.normalizedAge);
        case VisiaPrimitiveKind::Invalid:
            break;
    }
    throw std::invalid_argument("unknown Visia primitive kind");
}

}  // namespace konbini::render
