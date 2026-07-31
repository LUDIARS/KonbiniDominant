#include "konbini/render/resident_visual_geometry.h"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

// @implements spec/interface/visia-presentation.md Pictor integration boundary
// @implements spec/feature/npc-conversations-and-placement-feedback.md Visia dummy

namespace konbini::render {

namespace {

constexpr double kRadiansToDegrees =
    57.295779513082320876798154814105;

void appendGeometry(VisiaGeometry& destination,
                    const VisiaGeometry& source) {
    const std::size_t maximum =
        static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max());
    if (destination.vertices.size() >
        maximum - source.vertices.size()) {
        throw std::overflow_error("resident visual vertex index overflow");
    }
    const std::uint32_t base =
        static_cast<std::uint32_t>(destination.vertices.size());
    destination.vertices.insert(destination.vertices.end(),
                                source.vertices.begin(),
                                source.vertices.end());
    if (destination.indices.size() >
        destination.indices.max_size() - source.indices.size()) {
        throw std::overflow_error("resident visual index count overflow");
    }
    destination.indices.reserve(destination.indices.size() +
                                source.indices.size());
    for (const std::uint32_t index : source.indices) {
        if (index >= source.vertices.size() ||
            index > std::numeric_limits<std::uint32_t>::max() - base) {
            throw std::logic_error("resident visual source index is invalid");
        }
        destination.indices.push_back(base + index);
    }
}

[[nodiscard]] SpeechBubbleRequest makeSpeechRequest(
    const sim::ResidentPresentation& resident) {
    if (!resident.speech.has_value()) {
        throw std::logic_error("resident speech request has no text");
    }
    const ResidentPrimitiveVisiaDefinition& residentVisia =
        residentPrimitiveVisia();
    const double headTopMeters =
        residentVisia.head.centerMeters.y +
        residentVisia.head.halfExtentsMeters.y;
    if (!sim::isFinite(resident.positionMeters) ||
        !std::isfinite(resident.bubble.heightMeters) ||
        resident.bubble.heightMeters <= headTopMeters ||
        !std::isfinite(resident.bubble.maxDistanceMeters) ||
        resident.bubble.maxDistanceMeters <= 0.0) {
        throw std::invalid_argument("invalid resident speech presentation");
    }
    return {
        .anchorMeters = {
            resident.positionMeters.x,
            resident.positionMeters.y + resident.bubble.heightMeters,
            resident.positionMeters.z,
        },
        .text = std::string_view(*resident.speech),
        .hideDistanceMeters = resident.bubble.maxDistanceMeters,
    };
}

}  // namespace

// @implements spec/feature/npc-conversations-and-placement-feedback.md Visia dummy
ResidentVisualGeometry buildResidentVisualGeometry(
    const std::span<const sim::ResidentPresentation> residents,
    const IsometricCamera& camera,
    const SpeechBubbleStyle& speechStyle) {
    ResidentVisualGeometry output;
    std::vector<SpeechBubbleRequest> speechRequests;
    speechRequests.reserve(residents.size());

    for (const sim::ResidentPresentation& resident : residents) {
        if (!sim::isFinite(resident.positionMeters) ||
            !std::isfinite(resident.yawRadians)) {
            throw std::invalid_argument("invalid resident visual transform");
        }
        const VisiaInstance instance{
            .id = VisiaId::ResidentPrimitive,
            .pose = {
                .positionMeters = resident.positionMeters,
                .yawDegrees = resident.yawRadians * kRadiansToDegrees,
            },
            .normalizedAge = 0.0,
        };
        appendGeometry(output.residents,
                       buildPrimitiveVisiaGeometry(instance));
        if (resident.speech.has_value()) {
            speechRequests.push_back(makeSpeechRequest(resident));
        }
    }

    output.speechBubbles = buildSpeechBubbleGeometry(
        camera, speechRequests, speechStyle);
    return output;
}

}  // namespace konbini::render
