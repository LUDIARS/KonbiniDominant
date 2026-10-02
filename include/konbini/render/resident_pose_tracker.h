#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "konbini/sim/resident_presentation.h"

// @implements spec/feature/npc-conversations-and-placement-feedback.md Runtime resident sync

namespace konbini::render {

struct ResidentPoseTrackerSpec {
    // Seconds to blend from the last displayed pose to a re-targeted
    // resident's new snapshot pose (BASE-NPC-RETARGET-01).
    double retargetBlendSeconds = 0.6;
};

void validateResidentPoseTrackerSpec(const ResidentPoseTrackerSpec& spec);

// The pose a resident is displayed at this frame.
struct TrackedResidentPose {
    sim::ResidentPresentationId id{};
    sim::Vec3 positionMeters{};
    double yawDegrees = 0.0;
    std::optional<std::string> speech;
    sim::ResidentBubbleConfig bubble{};
};

// Presentation-only resident state keyed by the stable resident id.
//
// - A resident that appears is shown at its snapshot pose.
// - A resident whose destination changed (ZOC re-assignment: different
//   `targetStore` or `route`) blends from the pose it was displayed at to the
//   new snapshot pose over `retargetBlendSeconds`, so it never teleports.
// - A resident that leaves the snapshot is dropped in the same observe.
// - A snapshot tick that goes backwards (load / reset) drops every blend.
//
// Nothing here is written back to the simulation or the snapshot.
class ResidentPoseTracker {
public:
    explicit ResidentPoseTracker(
        ResidentPoseTrackerSpec spec = ResidentPoseTrackerSpec{});

    // Repeated observes of the same tick are ignored.
    void observe(std::span<const sim::ResidentPresentation> residents,
                 std::uint64_t snapshotTick);
    // Finite, non-negative presentation seconds.
    void advance(double deltaSeconds);
    void reset();

    // Displayed poses in ascending resident id order.
    [[nodiscard]] const std::vector<TrackedResidentPose>& poses() const noexcept;
    [[nodiscard]] std::size_t blendingCount() const noexcept;

private:
    struct Track {
        sim::ResidentPresentation target;
        sim::Vec3 fromPosition{};
        double fromYawDegrees = 0.0;
        // Unset when the resident is not blending.
        std::optional<double> blendElapsed;
    };

    void refresh();

    ResidentPoseTrackerSpec spec_;
    std::map<sim::ResidentPresentationId, Track> tracks_;
    std::optional<std::uint64_t> lastTick_;
    std::vector<TrackedResidentPose> poses_;
};

}  // namespace konbini::render
