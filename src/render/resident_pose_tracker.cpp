#include "konbini/render/resident_pose_tracker.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

// @implements spec/feature/npc-conversations-and-placement-feedback.md Runtime resident sync

namespace konbini::render {

namespace {

constexpr double kRadiansToDegrees = 57.295779513082320876798154814105;

[[nodiscard]] double smoothstep(const double t) noexcept {
    const double x = std::clamp(t, 0.0, 1.0);
    return x * x * (3.0 - 2.0 * x);
}

// Shortest signed turn from `from` to `to`, in (-180, 180].
[[nodiscard]] double yawDelta(const double from, const double to) noexcept {
    double delta = std::fmod(to - from, 360.0);
    if (delta > 180.0) {
        delta -= 360.0;
    } else if (delta <= -180.0) {
        delta += 360.0;
    }
    return delta;
}

void validateResident(const sim::ResidentPresentation& resident) {
    if (!resident.id.populationCellId.isValid() ||
        !sim::isFinite(resident.positionMeters) ||
        !std::isfinite(resident.yawRadians)) {
        throw std::invalid_argument(
            "resident pose tracker received an invalid resident");
    }
}

}  // namespace

void validateResidentPoseTrackerSpec(const ResidentPoseTrackerSpec& spec) {
    if (!std::isfinite(spec.retargetBlendSeconds) ||
        spec.retargetBlendSeconds <= 0.0) {
        throw std::invalid_argument("resident retarget blend must be positive");
    }
}

ResidentPoseTracker::ResidentPoseTracker(const ResidentPoseTrackerSpec spec)
    : spec_(spec) {
    validateResidentPoseTrackerSpec(spec_);
}

// @implements spec/feature/npc-conversations-and-placement-feedback.md Runtime resident sync
void ResidentPoseTracker::observe(
    const std::span<const sim::ResidentPresentation> residents,
    const std::uint64_t snapshotTick) {
    if (lastTick_.has_value() && snapshotTick == *lastTick_) {
        return;
    }
    for (const sim::ResidentPresentation& resident : residents) {
        validateResident(resident);
    }
    if (lastTick_.has_value() && snapshotTick < *lastTick_) {
        tracks_.clear();
        poses_.clear();
    }
    lastTick_ = snapshotTick;

    // The poses displayed last frame are the blend sources.
    std::map<sim::ResidentPresentationId, const TrackedResidentPose*> displayed;
    for (const TrackedResidentPose& pose : poses_) {
        displayed.emplace(pose.id, &pose);
    }

    std::map<sim::ResidentPresentationId, Track> next;
    for (const sim::ResidentPresentation& resident : residents) {
        Track track{.target = resident};
        const auto previous = tracks_.find(resident.id);
        if (previous != tracks_.end()) {
            const bool retargeted =
                previous->second.target.targetStore != resident.targetStore ||
                previous->second.target.route != resident.route;
            if (retargeted) {
                const TrackedResidentPose& shown = *displayed.at(resident.id);
                track.fromPosition = shown.positionMeters;
                track.fromYawDegrees = shown.yawDegrees;
                track.blendElapsed = 0.0;
            } else {
                track.fromPosition = previous->second.fromPosition;
                track.fromYawDegrees = previous->second.fromYawDegrees;
                track.blendElapsed = previous->second.blendElapsed;
            }
        }
        if (!next.emplace(resident.id, std::move(track)).second) {
            throw std::invalid_argument(
                "resident pose tracker received a duplicate resident id");
        }
    }
    tracks_ = std::move(next);
    refresh();
}

void ResidentPoseTracker::advance(const double deltaSeconds) {
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0) {
        throw std::invalid_argument(
            "resident pose tracker requires a finite non-negative delta");
    }
    for (auto& entry : tracks_) {
        Track& track = entry.second;
        if (!track.blendElapsed.has_value()) {
            continue;
        }
        *track.blendElapsed += deltaSeconds;
        if (*track.blendElapsed >= spec_.retargetBlendSeconds) {
            track.blendElapsed.reset();
        }
    }
    refresh();
}

void ResidentPoseTracker::reset() {
    tracks_.clear();
    poses_.clear();
    lastTick_.reset();
}

const std::vector<TrackedResidentPose>& ResidentPoseTracker::poses()
    const noexcept {
    return poses_;
}

std::size_t ResidentPoseTracker::blendingCount() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        tracks_.begin(), tracks_.end(), [](const auto& entry) {
            return entry.second.blendElapsed.has_value();
        }));
}

void ResidentPoseTracker::refresh() {
    std::vector<TrackedResidentPose> poses;
    poses.reserve(tracks_.size());
    for (const auto& [id, track] : tracks_) {
        const sim::ResidentPresentation& target = track.target;
        TrackedResidentPose pose{
            .id = id,
            .positionMeters = target.positionMeters,
            .yawDegrees = target.yawRadians * kRadiansToDegrees,
            .speech = target.speech,
            .bubble = target.bubble,
        };
        if (track.blendElapsed.has_value()) {
            const double s =
                smoothstep(*track.blendElapsed / spec_.retargetBlendSeconds);
            const sim::Vec3& from = track.fromPosition;
            const sim::Vec3& to = target.positionMeters;
            pose.positionMeters = {
                from.x + (to.x - from.x) * s,
                from.y + (to.y - from.y) * s,
                from.z + (to.z - from.z) * s,
            };
            pose.yawDegrees =
                track.fromYawDegrees +
                yawDelta(track.fromYawDegrees, pose.yawDegrees) * s;
        }
        poses.push_back(std::move(pose));
    }
    poses_ = std::move(poses);
}

}  // namespace konbini::render
