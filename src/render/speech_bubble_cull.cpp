#include "konbini/render/speech_bubble_cull.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "konbini/render/visia.h"

// @implements spec/feature/npc-conversations-and-placement-feedback.md Bubble culling

namespace konbini::render {

namespace {

struct Projected {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    bool valid = false;
};

[[nodiscard]] Projected project(
    const std::array<float, 16>& m, const sim::Vec3& p) noexcept {
    const double x = m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12];
    const double y = m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13];
    const double z = m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14];
    const double w = m[3] * p.x + m[7] * p.y + m[11] * p.z + m[15];
    if (!(w > 0.0) || !std::isfinite(w)) {
        return {};
    }
    return {.x = x / w, .y = y / w, .z = z / w, .valid = true};
}

[[nodiscard]] double headTopMeters() {
    const ResidentPrimitiveVisiaDefinition& resident = residentPrimitiveVisia();
    return resident.head.centerMeters.y + resident.head.halfExtentsMeters.y;
}

}  // namespace

void validateSpeechBubbleCullSpec(const SpeechBubbleCullSpec& spec) {
    if (spec.maxVisibleBubbles == 0 || !std::isfinite(spec.screenMarginNdc) ||
        spec.screenMarginNdc < 0.0) {
        throw std::invalid_argument("invalid speech bubble cull spec");
    }
}

// @implements spec/feature/npc-conversations-and-placement-feedback.md Bubble culling
SpeechBubbleCullResult cullSpeechBubbles(
    const IsometricCamera& camera,
    const std::span<const TrackedResidentPose> poses,
    const SpeechBubbleCullSpec& spec) {
    validateSpeechBubbleCullSpec(spec);
    if (!sim::isFinite(camera.eyeMeters)) {
        throw std::invalid_argument("speech bubble cull needs a finite camera");
    }
    const double headTop = headTopMeters();
    const double limit = 1.0 + spec.screenMarginNdc;

    SpeechBubbleCullResult result;
    for (const TrackedResidentPose& pose : poses) {
        if (!pose.speech.has_value()) {
            continue;
        }
        if (pose.speech->empty() || !std::isfinite(pose.bubble.heightMeters) ||
            pose.bubble.heightMeters <= headTop ||
            !std::isfinite(pose.bubble.maxDistanceMeters) ||
            pose.bubble.maxDistanceMeters <= 0.0 ||
            !sim::isFinite(pose.positionMeters)) {
            throw std::invalid_argument("invalid resident speech presentation");
        }
        const sim::Vec3 anchor{
            pose.positionMeters.x,
            pose.positionMeters.y + pose.bubble.heightMeters,
            pose.positionMeters.z,
        };
        const double dx = anchor.x - camera.eyeMeters.x;
        const double dy = anchor.y - camera.eyeMeters.y;
        const double dz = anchor.z - camera.eyeMeters.z;
        const double distanceSquared = dx * dx + dy * dy + dz * dz;
        const double maxDistance = pose.bubble.maxDistanceMeters;
        if (distanceSquared > maxDistance * maxDistance) {
            ++result.culledByDistance;
            continue;
        }
        const Projected ndc = project(camera.viewProjection, anchor);
        if (!ndc.valid || std::abs(ndc.x) > limit || std::abs(ndc.y) > limit ||
            ndc.z < 0.0 || ndc.z > 1.0) {
            ++result.culledOffscreen;
            continue;
        }
        result.visible.push_back({
            .id = pose.id,
            .anchorMeters = anchor,
            .distanceMeters = std::sqrt(distanceSquared),
            .lineKey = *pose.speech,
        });
    }

    std::sort(result.visible.begin(), result.visible.end(),
              [](const VisibleSpeechBubble& left,
                 const VisibleSpeechBubble& right) {
                  if (left.distanceMeters != right.distanceMeters) {
                      return left.distanceMeters < right.distanceMeters;
                  }
                  return left.id < right.id;
              });
    if (result.visible.size() > spec.maxVisibleBubbles) {
        result.culledByLimit = result.visible.size() - spec.maxVisibleBubbles;
        result.visible.resize(spec.maxVisibleBubbles);
    }
    return result;
}

}  // namespace konbini::render
