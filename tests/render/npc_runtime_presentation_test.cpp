#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "../check.h"

#include "konbini/render/animated_store_geometry.h"
#include "konbini/render/isometric_camera.h"
#include "konbini/render/npc_presentation_draws.h"
#include "konbini/render/presentation_meshes.h"
#include "konbini/render/presentation_transform.h"
#include "konbini/render/resident_pose_tracker.h"
#include "konbini/render/speech_bubble_cull.h"
#include "konbini/render/speech_bubble_layout.h"
#include "konbini/render/speech_glyph_atlas.h"
#include "konbini/render/speech_line_catalog.h"
#include "konbini/render/store_placement_animation.h"
#include "konbini/render/visia_geometry.h"

// KD-NPC-002 game-owned half: resident pose sync, bubble culling, the
// Japanese speech atlas / catalog, bubble layout and the presentation draw
// list. No Pictor or Vulkan dependency; the Pictor object half is covered by
// konbini_pictor_presentation_tests.
//
// @implements spec/test/verification-strategy.md 5. Pictor / Ergo integration
// @implements spec/feature/npc-conversations-and-placement-feedback.md Runtime resident sync

namespace {

namespace r = konbini::render;
namespace s = konbini::sim;

s::PopulationCellId cell(const std::uint32_t index) {
    return s::PopulationCellId{{index, 1}};
}

s::StoreId store(const std::uint32_t index) {
    return s::StoreId{{index, 1}};
}

s::ResidentPresentation resident(const std::uint32_t index, const s::Vec3 position,
                                 const std::uint32_t storeIndex,
                                 const double yawRadians = 0.0) {
    s::ResidentPresentation value;
    value.id = {cell(index), 0};
    value.phase = s::ResidentTripPhase::WalkingToStore;
    value.positionMeters = position;
    value.yawRadians = yawRadians;
    value.targetStore = store(storeIndex);
    value.route = s::ResidentRouteState::DirectLine;
    value.bubble = {.heightMeters = 2.2, .maxDistanceMeters = 220.0};
    return value;
}

r::IsometricCamera camera(const s::Vec3 target = {}) {
    r::IsometricCameraConfig config;
    config.targetMeters = target;
    config.distanceMeters = 180.0;
    config.verticalSpanMeters = 160.0;
    return r::buildIsometricCamera(config, {1280, 720});
}

bool near(const s::Vec3& a, const s::Vec3& b, const double tolerance = 1e-4) {
    return std::abs(a.x - b.x) <= tolerance && std::abs(a.y - b.y) <= tolerance &&
           std::abs(a.z - b.z) <= tolerance;
}

// --- speech atlas / catalog ----------------------------------------------

void catalogResolvesContentKeysToJapaneseLines() {
    CHECK(r::speechLocale() == "ja");
    CHECK(r::speechLines().size() == 3);
    const r::SpeechLine& line = r::speechLine("NICE AND CLOSE");
    // 近くて便利！ (U+8FD1 ... U+FF01)
    const std::vector<char32_t> codepoints = r::decodeUtf8(line.text);
    CHECK(codepoints.size() == 6);
    CHECK(!codepoints.empty() && codepoints.front() == U'近');
    CHECK(!codepoints.empty() && codepoints.back() == U'！');
    // Every catalog line is drawable from the baked subset only.
    for (const r::SpeechLine& entry : r::speechLines()) {
        for (const char32_t codepoint : r::decodeUtf8(entry.text)) {
            CHECK_NO_THROW(static_cast<void>(r::speechGlyph(codepoint)));
        }
    }
    const std::array<std::string, 3> remarks{"NICE AND CLOSE", "EASY TO REACH",
                                             "HANDY LOCATION"};
    CHECK_NO_THROW(r::validateSpeechLineKeys(remarks));
    const std::array<std::string, 1> unknown{"STORE 24H"};
    CHECK_THROWS(std::out_of_range, r::validateSpeechLineKeys(unknown));
    CHECK_THROWS(std::out_of_range, static_cast<void>(r::speechLine("")));
}

void atlasIsASubsetWithoutFallback() {
    // Only the catalog's code points are baked; a full CJK font is not.
    CHECK(r::speechGlyphs().size() < 64);
    CHECK(std::is_sorted(r::speechGlyphs().begin(), r::speechGlyphs().end(),
                         [](const r::SpeechGlyph& a, const r::SpeechGlyph& b) {
                             return a.codepoint < b.codepoint;
                         }));
    CHECK_THROWS(std::out_of_range, static_cast<void>(r::speechGlyph(U'A')));
    CHECK_THROWS(std::out_of_range, static_cast<void>(r::speechGlyph(U'猫')));

    const r::SpeechGlyph& glyph = r::speechGlyph(U'便');  // 便
    const r::WorldMesh mesh = r::buildSpeechGlyphMesh(glyph, {1, 1, 1, 1});
    CHECK(!mesh.indices.empty() && mesh.indices.size() % 3 == 0);
    float minY = 10.0F;
    float maxY = -10.0F;
    for (const r::WorldVertex& vertex : mesh.vertices) {
        minY = std::min(minY, vertex.position[1]);
        maxY = std::max(maxY, vertex.position[1]);
        CHECK(vertex.position[2] == 0.0F);
        CHECK((vertex.normal == r::WorldVertex::Normal{0.0F, 0.0F, 0.0F}));
    }
    // The CJK em box spans roughly [-0.12, 0.88] around the baseline.
    CHECK(minY > -0.2F && minY < 0.0F);
    CHECK(maxY > 0.7F && maxY < 0.95F);
}

void utf8DecodeIsStrict() {
    CHECK(r::decodeUtf8("A") == std::vector<char32_t>{U'A'});
    CHECK(r::decodeUtf8("\xE3\x81\x84") == std::vector<char32_t>{U'い'});
    CHECK_THROWS(std::invalid_argument, static_cast<void>(r::decodeUtf8("\xE3\x81")));
    CHECK_THROWS(std::invalid_argument, static_cast<void>(r::decodeUtf8("\xC0\x80")));
    CHECK_THROWS(std::invalid_argument, static_cast<void>(r::decodeUtf8("\xED\xA0\x80")));
    CHECK_THROWS(std::invalid_argument, static_cast<void>(r::decodeUtf8("\xFF")));
}

// --- resident pose tracker ----------------------------------------------

void trackerShowsNewResidentsAtTheirSnapshotPose() {
    r::ResidentPoseTracker tracker;
    const std::vector<s::ResidentPresentation> residents{
        resident(2, {5, 0, 5}, 1), resident(1, {1, 0, 1}, 1)};
    tracker.observe(residents, 10);
    CHECK(tracker.poses().size() == 2);
    CHECK(tracker.blendingCount() == 0);
    // Ascending stable id order regardless of snapshot order.
    CHECK(tracker.poses()[0].id.populationCellId == cell(1));
    CHECK(near(tracker.poses()[1].positionMeters, {5, 0, 5}));
}

void trackerBlendsARetargetedResidentFromItsDisplayedPose() {
    r::ResidentPoseTracker tracker({.retargetBlendSeconds = 1.0});
    tracker.observe(std::vector{resident(1, {0, 0, 0}, 1)}, 10);
    tracker.observe(std::vector{resident(1, {1, 0, 0}, 1)}, 11);
    // Same destination: the walk follows the snapshot without a blend.
    CHECK(tracker.blendingCount() == 0);
    CHECK(near(tracker.poses()[0].positionMeters, {1, 0, 0}));

    // ZOC re-assignment: new target store and a far-away snapshot pose.
    tracker.observe(std::vector{resident(1, {41, 0, 0}, 2)}, 12);
    CHECK(tracker.blendingCount() == 1);
    // No teleport: the first frame still shows the previous pose.
    CHECK(near(tracker.poses()[0].positionMeters, {1, 0, 0}));
    tracker.advance(0.5);
    const double halfway = tracker.poses()[0].positionMeters.x;
    CHECK(halfway > 1.0 && halfway < 41.0);
    CHECK(std::abs(halfway - 21.0) < 1e-9);  // smoothstep(0.5) == 0.5
    tracker.advance(0.5);
    CHECK(tracker.blendingCount() == 0);
    CHECK(near(tracker.poses()[0].positionMeters, {41, 0, 0}));
}

void trackerRetargetsOnRouteChangeAndTurnsTheShortWay() {
    r::ResidentPoseTracker tracker({.retargetBlendSeconds = 1.0});
    constexpr double kDegrees = 0.017453292519943295;
    tracker.observe(std::vector{resident(1, {0, 0, 0}, 1, 170.0 * kDegrees)}, 1);
    s::ResidentPresentation rerouted = resident(1, {0, 0, 0}, 1, -170.0 * kDegrees);
    rerouted.route = s::ResidentRouteState::PedestrianPath;
    tracker.observe(std::vector{rerouted}, 2);
    CHECK(tracker.blendingCount() == 1);
    tracker.advance(0.5);
    // 170 -> -170 turns through 180, not back through 0.
    const double yaw = tracker.poses()[0].yawDegrees;
    CHECK(std::abs(std::abs(yaw) - 180.0) < 1e-6);
}

void trackerDropsRemovedResidentsAndResetsOnRewind() {
    r::ResidentPoseTracker tracker;
    tracker.observe(std::vector{resident(1, {0, 0, 0}, 1), resident(2, {2, 0, 0}, 1)}, 5);
    tracker.observe(std::vector{resident(2, {3, 0, 0}, 1)}, 6);
    CHECK(tracker.poses().size() == 1);
    CHECK(tracker.poses()[0].id.populationCellId == cell(2));

    tracker.observe(std::vector{resident(2, {50, 0, 0}, 9)}, 7);
    CHECK(tracker.blendingCount() == 1);
    // Same tick again is ignored.
    tracker.observe(std::vector<s::ResidentPresentation>{}, 7);
    CHECK(tracker.poses().size() == 1);
    // A rewind (load / retry) restarts without blending from stale poses.
    tracker.observe(std::vector{resident(2, {9, 0, 0}, 3)}, 1);
    CHECK(tracker.blendingCount() == 0);
    CHECK(near(tracker.poses()[0].positionMeters, {9, 0, 0}));

    CHECK_THROWS(std::invalid_argument,
                 tracker.observe(std::vector{resident(4, {0, 0, 0}, 1),
                                             resident(4, {1, 0, 0}, 1)},
                                 2));
    CHECK_THROWS(std::invalid_argument, tracker.advance(-1.0));
    CHECK_THROWS(std::invalid_argument,
                 r::ResidentPoseTracker({.retargetBlendSeconds = 0.0}));
}

// --- bubble culling -------------------------------------------------------

std::vector<r::TrackedResidentPose> speakingPoses(const std::vector<s::Vec3>& positions) {
    std::vector<r::TrackedResidentPose> poses;
    std::uint32_t index = 1;
    for (const s::Vec3& position : positions) {
        poses.push_back({
            .id = {cell(index++), 0},
            .positionMeters = position,
            .speech = std::string("NICE AND CLOSE"),
            .bubble = {.heightMeters = 2.2, .maxDistanceMeters = 220.0},
        });
    }
    return poses;
}

void bubbleCullUsesDistanceScreenAndLimit() {
    const r::IsometricCamera view = camera();
    std::vector<r::TrackedResidentPose> poses = speakingPoses({
        {0, 0, 0}, {10, 0, 0}, {-10, 0, 0}, {5000, 0, 0},
    });
    // Off-screen but within distance: far to the side of the 160 m view.
    poses[3].bubble.maxDistanceMeters = 100000.0;
    // Silent residents never become bubbles.
    poses.push_back({.id = {cell(9), 0}, .positionMeters = {1, 0, 1}});

    r::SpeechBubbleCullResult result =
        r::cullSpeechBubbles(view, poses, {.maxVisibleBubbles = 2});
    CHECK(result.culledOffscreen == 1);
    CHECK(result.culledByLimit == 1);
    CHECK(result.visible.size() == 2);
    for (std::size_t i = 1; i < result.visible.size(); ++i) {
        CHECK(result.visible[i - 1].distanceMeters <= result.visible[i].distanceMeters);
    }
    if (!result.visible.empty()) {
        CHECK(result.visible[0].lineKey == "NICE AND CLOSE");
        CHECK(std::abs(result.visible[0].anchorMeters.y - 2.2) < 1e-12);
    }

    // Distance cull: the camera eye is ~180 m away.
    poses = speakingPoses({{0, 0, 0}});
    poses[0].bubble.maxDistanceMeters = 50.0;
    result = r::cullSpeechBubbles(view, poses, {});
    CHECK(result.visible.empty() && result.culledByDistance == 1);

    // Anchor inside the head is a content error, not a hidden bubble.
    poses[0].bubble = {.heightMeters = 1.0, .maxDistanceMeters = 220.0};
    CHECK_THROWS(std::invalid_argument,
                 static_cast<void>(r::cullSpeechBubbles(view, poses, {})));
    CHECK_THROWS(std::invalid_argument,
                 static_cast<void>(r::cullSpeechBubbles(
                     view, poses, {.maxVisibleBubbles = 0})));
}

// --- bubble layout ----------------------------------------------------------

void bubbleLayoutDrawsTheLocalizedLineOnTheCameraPlane() {
    const r::IsometricCamera view = camera();
    const r::VisibleSpeechBubble bubble{
        .id = {cell(3), 2},
        .anchorMeters = {4, 2.2, -3},
        .distanceMeters = 180,
        .lineKey = "EASY TO REACH",
    };
    std::vector<r::PresentationDraw> draws;
    r::appendSpeechBubbleDraws(view, bubble, {}, draws);
    // background + tail + 6 glyphs (すぐ行けるね)
    CHECK(draws.size() == 8);
    std::set<r::PresentationObjectKey> keys;
    for (const r::PresentationDraw& draw : draws) {
        keys.insert(draw.key);
        CHECK(!draw.translucent);
    }
    CHECK(keys.size() == draws.size());
    if (draws.size() == 8) {
        CHECK(draws[0].mesh.kind == r::PresentationMeshKind::BubbleBackground);
        CHECK(draws[1].mesh.kind == r::PresentationMeshKind::BubbleTail);
        // The tail tip (local (0, -1)) lands exactly on the head anchor.
        CHECK(near(r::transformPoint(draws[1].model, {0, -1, 0}), bubble.anchorMeters));
        CHECK(draws[2].mesh.kind == r::PresentationMeshKind::SpeechGlyph);
        CHECK(draws[2].mesh.index == 0x3059);  // す
        // Glyphs advance along the camera right axis.
        const s::Vec3 first = r::transformPoint(draws[2].model, {0, 0, 0});
        const s::Vec3 second = r::transformPoint(draws[3].model, {0, 0, 0});
        const double along = (second.x - first.x) * view.right.x +
                             (second.y - first.y) * view.right.y +
                             (second.z - first.z) * view.right.z;
        CHECK(along > 0.0);
    }
    r::VisibleSpeechBubble unknown = bubble;
    unknown.lineKey = "MISSING";
    CHECK_THROWS(std::out_of_range, r::appendSpeechBubbleDraws(view, unknown, {}, draws));
    r::SpeechBubbleLayoutStyle tight;
    tight.maxGlyphs = 3;
    CHECK_THROWS(std::invalid_argument, r::appendSpeechBubbleDraws(view, bubble, tight, draws));
}

// --- presentation meshes / draws -----------------------------------------

void residentInstancesMatchTheVisiaGeometry() {
    const std::vector<r::PresentationMesh> meshes = r::buildPresentationMeshes();
    const auto residentMeshes = std::count_if(meshes.begin(), meshes.end(), [](const auto& m) {
        return m.key.kind == r::PresentationMeshKind::ResidentBody;
    });
    CHECK(residentMeshes == 1);
    const auto ringFrames = std::count_if(meshes.begin(), meshes.end(), [](const auto& m) {
        return m.key.kind == r::PresentationMeshKind::LandingRingFrame;
    });
    CHECK(ringFrames == r::kLandingRingFrameCount);
    std::set<r::PresentationMeshKey> keys;
    for (const auto& mesh : meshes) {
        keys.insert(mesh.key);
    }
    CHECK(keys.size() == meshes.size());

    // Shared mesh + per-object model == the CPU Visia geometry at that pose.
    const r::VisiaPose pose{{12.5, 3.0, -4.0}, 37.0};
    const r::WorldMesh expected =
        r::buildResidentPrimitiveGeometry(r::residentPrimitiveVisia(), pose);
    const r::WorldMesh& shared = meshes.front().mesh;
    CHECK(shared.vertices.size() == expected.vertices.size());
    const r::ColumnMajorMatrix model =
        r::yawTranslationModel(pose.positionMeters, pose.yawDegrees);
    for (std::size_t i = 0; i < std::min(shared.vertices.size(), expected.vertices.size()); ++i) {
        const auto& p = shared.vertices[i].position;
        const auto& q = expected.vertices[i].position;
        CHECK(near(r::transformPoint(model, {p[0], p[1], p[2]}), {q[0], q[1], q[2]}, 1e-3));
    }

    CHECK(r::landingRingFrame(0.0) == 0);
    CHECK(r::landingRingFrame(1.0) == r::kLandingRingFrameCount - 1);
    CHECK_THROWS(std::invalid_argument, static_cast<void>(r::landingRingFrame(1.5)));
}

r::StoreConstructionVisual constructionAt(const std::uint32_t storeIndex,
                                          const double elapsedSeconds) {
    r::StoreConstructionVisual visual;
    visual.store.id = store(storeIndex);
    visual.store.positionMeters = {20, 0, 20};
    visual.elapsedSeconds = elapsedSeconds;
    visual.animation = r::sampleStorePlacementAnimation(
        r::defaultStorePlacementAnimationSpec(), {visual.store.positionMeters, 0},
        elapsedSeconds);
    return visual;
}

void presentationDrawsShareMeshesAndFollowEffectLifecycle() {
    const r::IsometricCamera view = camera();
    r::ResidentPoseTracker tracker;
    s::ResidentPresentation speaker = resident(1, {0, 0, 0}, 1);
    speaker.speech = "HANDY LOCATION";
    tracker.observe(std::vector{speaker, resident(2, {6, 0, 0}, 1)}, 1);

    const r::StorePlacementAnimationSpec spec = r::defaultStorePlacementAnimationSpec();
    const double landed = spec.spinSeconds + spec.holdSeconds + spec.fallSeconds;
    const std::vector<r::StoreConstructionVisual> airborne{constructionAt(7, 0.1)};
    const std::vector<r::StoreConstructionVisual> landing{
        constructionAt(7, landed + spec.effectSeconds * 0.5)};

    r::NpcPresentationFrame frame =
        r::buildNpcPresentationDraws(tracker.poses(), airborne, view, {});
    std::size_t residentDraws = 0;
    std::size_t effects = 0;
    std::set<r::PresentationObjectKey> keys;
    for (const r::PresentationDraw& draw : frame.draws) {
        keys.insert(draw.key);
        if (draw.key.role == r::PresentationObjectRole::Resident) {
            ++residentDraws;
            CHECK(draw.mesh == (r::PresentationMeshKey{r::PresentationMeshKind::ResidentBody, 0}));
        }
        effects += draw.key.role == r::PresentationObjectRole::LandingEffect ? 1U : 0U;
    }
    CHECK(keys.size() == frame.draws.size());
    CHECK(residentDraws == 2);
    // Before impact the store is airborne and no landing effect exists yet.
    CHECK(effects == 0);
    CHECK(frame.bubbles.visible.size() == 1);

    frame = r::buildNpcPresentationDraws(tracker.poses(), landing, view, {});
    const auto ring = std::find_if(frame.draws.begin(), frame.draws.end(), [](const auto& d) {
        return d.key.role == r::PresentationObjectRole::LandingEffect;
    });
    CHECK(ring != frame.draws.end());
    if (ring != frame.draws.end()) {
        CHECK(ring->translucent);
        CHECK(ring->mesh.kind == r::PresentationMeshKind::LandingRingFrame);
        CHECK(ring->mesh.index > 0 && ring->mesh.index < r::kLandingRingFrameCount - 1);
        CHECK(near(r::transformPoint(ring->model, {0, 0, 0}), {20, 0, 20}));
    }

    // Expired effect: the visual is gone, so is the ring.
    frame = r::buildNpcPresentationDraws(tracker.poses(), {}, view, {});
    CHECK(std::none_of(frame.draws.begin(), frame.draws.end(), [](const auto& d) {
        return d.key.role == r::PresentationObjectRole::LandingEffect;
    }));
}

void missedCueStillDrawsTheStoreAtItsFinalPose() {
    // A store whose cue never reached the presenter (overflow, skipped
    // snapshot) has no construction visual: it is drawn settled, exactly
    // as the snapshot transform, and no landing ring is started.
    s::RenderStore settled;
    settled.id = store(3);
    settled.facilityId = s::FacilityId{{3, 1}};
    settled.positionMeters = {8, 0, -8};
    const std::vector<s::RenderStore> stores{settled};
    const r::StoreMarkerSpec spec;
    const r::WorldMesh animated = r::buildAnimatedStoreGeometry(stores, spec, {});
    const r::WorldMesh direct = r::buildStoreMarkerGeometry(stores, spec);
    CHECK(animated.vertices.size() == direct.vertices.size());
    CHECK(animated.indices == direct.indices);
    for (std::size_t i = 0; i < std::min(animated.vertices.size(), direct.vertices.size()); ++i) {
        CHECK(animated.vertices[i].position == direct.vertices[i].position);
    }
    const r::NpcPresentationFrame frame =
        r::buildNpcPresentationDraws({}, {}, camera(), {});
    CHECK(frame.draws.empty());
}

}  // namespace

int main() {
    catalogResolvesContentKeysToJapaneseLines();
    atlasIsASubsetWithoutFallback();
    utf8DecodeIsStrict();
    trackerShowsNewResidentsAtTheirSnapshotPose();
    trackerBlendsARetargetedResidentFromItsDisplayedPose();
    trackerRetargetsOnRouteChangeAndTurnsTheShortWay();
    trackerDropsRemovedResidentsAndResetsOnRewind();
    bubbleCullUsesDistanceScreenAndLimit();
    bubbleLayoutDrawsTheLocalizedLineOnTheCameraPlane();
    residentInstancesMatchTheVisiaGeometry();
    presentationDrawsShareMeshesAndFollowEffectLifecycle();
    missedCueStillDrawsTheStoreAtItsFinalPose();
    return konbini::test::summarize("konbini_npc_runtime_presentation_tests");
}
