#include "konbini/render/npc_presentation_draws.h"

#include <stdexcept>

#include "konbini/render/presentation_meshes.h"
#include "konbini/render/presentation_object_keys.h"
#include "konbini/render/presentation_transform.h"

// @implements spec/feature/npc-conversations-and-placement-feedback.md Runtime resident sync

namespace konbini::render {

// @implements spec/feature/npc-conversations-and-placement-feedback.md Runtime resident sync
NpcPresentationFrame buildNpcPresentationDraws(
    const std::span<const TrackedResidentPose> residents,
    const std::span<const StoreConstructionVisual> construction,
    const IsometricCamera& camera, const NpcPresentationSpec& spec) {
    NpcPresentationFrame frame;
    frame.bubbles = cullSpeechBubbles(camera, residents, spec.bubbleCull);
    frame.draws.reserve(residents.size() + construction.size());

    for (const TrackedResidentPose& resident : residents) {
        frame.draws.push_back({
            .key = {PresentationObjectRole::Resident,
                    residentOwnerKey(resident.id), resident.id.ordinal},
            .mesh = {PresentationMeshKind::ResidentBody, 0},
            .model = yawTranslationModel(resident.positionMeters,
                                         resident.yawDegrees),
        });
    }

    for (const StoreConstructionVisual& visual : construction) {
        const auto& effect = visual.animation.landingEffect;
        if (!effect.has_value()) {
            continue;
        }
        if (effect->id != VisiaId::StoreLandingEffectPrimitive ||
            !sim::isFinite(effect->pose.positionMeters)) {
            throw std::invalid_argument(
                "placement sample carries an unsupported landing effect");
        }
        frame.draws.push_back({
            .key = {PresentationObjectRole::LandingEffect,
                    packEntityKey(visual.store.id.value()), 0},
            .mesh = {PresentationMeshKind::LandingRingFrame,
                     landingRingFrame(effect->normalizedAge)},
            .model = yawTranslationModel(effect->pose.positionMeters, 0.0),
            .translucent = true,
        });
    }

    for (const VisibleSpeechBubble& bubble : frame.bubbles.visible) {
        appendSpeechBubbleDraws(camera, bubble, spec.bubbleLayout, frame.draws);
    }
    return frame;
}

}  // namespace konbini::render
