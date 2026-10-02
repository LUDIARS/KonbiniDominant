#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include "../check.h"

#include "konbini/adapters/pictor/gpu_asset_store.h"
#include "konbini/adapters/pictor/gpu_mesh_key.h"
#include "konbini/adapters/pictor/pictor_batch_plan.h"
#include "konbini/adapters/pictor/pictor_frame_bridge.h"
#include "konbini/adapters/pictor/pictor_scene_sync.h"
#include "konbini/adapters/pictor/presentation_geometry_loader.h"
#include "konbini/adapters/pictor/presentation_object_sync.h"
#include "konbini/adapters/pictor/world_frustum.h"
#include "konbini/render/presentation_meshes.h"
#include "konbini/render/presentation_transform.h"
#include "pictor/batch/batch_builder.h"
#include "pictor/culling/culling_system.h"
#include "pictor/memory/memory_subsystem.h"
#include "pictor/scene/scene_registry.h"

// KD-NPC-002 Pictor half without a Vulkan device: shared presentation mesh
// upload, PresentationObjectKey <-> ObjectId lifecycle, mesh reference
// release, instancing through Pictor's real BatchBuilder and the frame
// bridge's consume path. Buffer handles are opaque fakes; the real Vulkan
// upload / record / present path is not covered by this binary
// (verification-strategy.md#5. Pictor / Ergo integration).
//
// @implements spec/test/verification-strategy.md 5. Pictor / Ergo integration
// @implements spec/interface/pictor-rendering.md Presentation objects

namespace {

namespace pa = konbini::adapters::pictor;
namespace r = konbini::render;
using konbini::render::WorldMesh;
using konbini::render::WorldVertex;

class FakeUploader final : public pa::IGpuMeshUploader {
public:
    pa::GpuMeshBuffers upload(const WorldMesh& mesh) override {
        ++nextId_;
        const pa::GpuMeshBuffers buffers{
            .vertexBuffer = reinterpret_cast<VkBuffer>(0x10000 + nextId_ * 2),
            .indexBuffer = reinterpret_cast<VkBuffer>(0x10001 + nextId_ * 2),
            .indexCount = static_cast<std::uint32_t>(mesh.indices.size()),
            .residentBytes = mesh.vertices.size() * sizeof(WorldVertex),
        };
        ++uploads;
        return buffers;
    }
    void release(const pa::GpuMeshBuffers&) noexcept override { ++releases; }

    int uploads = 0;
    int releases = 0;

private:
    std::uintptr_t nextId_ = 0;
};

::pictor::MemoryConfig smallMemory() {
    ::pictor::MemoryConfig config;
    config.frame_allocator_size = 1024U * 1024U;
    config.flight_count = 2;
    return config;
}

std::array<float, 16> wideOrtho() {
    std::array<float, 16> m{};
    m[0 * 4 + 0] = 0.01F;
    m[1 * 4 + 1] = 0.01F;
    m[2 * 4 + 2] = 0.005F;
    m[3 * 4 + 2] = 0.5F;
    m[3 * 4 + 3] = 1.0F;
    return m;
}

constexpr r::PresentationMeshKey kResidentMesh{r::PresentationMeshKind::ResidentBody, 0};

r::PresentationDraw residentDraw(const std::uint64_t owner, const konbini::sim::Vec3 position,
                                 const double yawDegrees = 0.0) {
    return {
        .key = {r::PresentationObjectRole::Resident, owner, 0},
        .mesh = kResidentMesh,
        .model = r::yawTranslationModel(position, yawDegrees),
    };
}

r::PresentationDraw ringDraw(const std::uint64_t owner, const std::uint32_t frame) {
    return {
        .key = {r::PresentationObjectRole::LandingEffect, owner, 0},
        .mesh = {r::PresentationMeshKind::LandingRingFrame, frame},
        .model = r::yawTranslationModel({5, 0, 5}, 0.0),
        .translucent = true,
    };
}

struct Fixture {
    FakeUploader uploader;
    pa::GpuAssetStore store;
    ::pictor::MemorySubsystem memory{smallMemory()};
    ::pictor::SceneRegistry registry{memory};

    Fixture() {
        store.initialize(uploader, 2);
        static_cast<void>(pa::loadPresentationGeometry(store));
    }

    [[nodiscard]] std::uint32_t references(const r::PresentationMeshKey key) const {
        return store.resolve(*store.find(pa::GpuMeshKey::presentation(key)))->references;
    }
};

void presentationMeshesUploadOnceInTheirOwnKeySpace() {
    Fixture fixture;
    const std::size_t expected = r::buildPresentationMeshes().size();
    CHECK(fixture.store.size() == expected);
    CHECK(fixture.uploader.uploads == static_cast<int>(expected));
    // Exactly one resident mesh, shared by every resident object.
    CHECK(fixture.store.contains(pa::GpuMeshKey::presentation(kResidentMesh)));
    // A Figmentum facility key with the same raw value is a different mesh.
    const pa::GpuMeshKey presentationKey = pa::GpuMeshKey::presentation(kResidentMesh);
    CHECK(!fixture.store.contains(konbini::sim::FigmentumFacilityKey{presentationKey.value}));
    // Uploading twice is a startup bug, not a silent re-upload.
    CHECK_THROWS(std::logic_error,
                 static_cast<void>(pa::loadPresentationGeometry(fixture.store)));
}

void residentObjectsFollowTheStableIdLifecycle() {
    Fixture fixture;
    pa::PresentationObjectSync sync(fixture.registry, fixture.store);
    std::vector<r::PresentationDraw> draws{residentDraw(1, {0, 0, 0}),
                                           residentDraw(2, {3, 0, 0})};
    pa::PresentationSyncReport report = sync.apply(draws, 1);
    CHECK(report.added == 2);
    CHECK(fixture.references(kResidentMesh) == 2);
    const auto first = sync.objectFor(draws[0].key);
    CHECK(first.has_value());

    // Walking: same object, new transform and bounds.
    draws[0] = residentDraw(1, {1, 0, 0}, 90.0);
    report = sync.apply(draws, 2);
    CHECK(report.moved == 1 && report.added == 0 && report.removed == 0);
    CHECK(sync.objectFor(draws[0].key) == first);
    if (first.has_value()) {
        const auto location = fixture.registry.find_object(*first);
        CHECK(location.valid);
        if (location.valid) {
            const ::pictor::ObjectPool& pool = fixture.registry.dynamic_pool();
            CHECK(pool.transforms()[location.pool_index].m[3][0] == 1.0F);
            const ::pictor::AABB& bounds = pool.bounds()[location.pool_index];
            CHECK(bounds.min.x < 1.0F && bounds.max.x > 1.0F);
            CHECK(bounds.max.y > 1.5F);
        }
    }

    // Resident 1 left the snapshot: its object and mesh reference go now.
    draws.erase(draws.begin());
    report = sync.apply(draws, 3);
    CHECK(report.removed == 1);
    CHECK(!sync.objectFor({r::PresentationObjectRole::Resident, 1, 0}).has_value());
    CHECK(fixture.references(kResidentMesh) == 1);
    CHECK(fixture.registry.total_object_count() == 1);

    sync.clear(4);
    CHECK(fixture.references(kResidentMesh) == 0);
    CHECK(fixture.registry.total_object_count() == 0);
}

void expiredEffectsReleaseTheirFrameMesh() {
    Fixture fixture;
    pa::PresentationObjectSync sync(fixture.registry, fixture.store);
    const r::PresentationMeshKey frame3{r::PresentationMeshKind::LandingRingFrame, 3};
    const r::PresentationMeshKey frame4{r::PresentationMeshKind::LandingRingFrame, 4};
    static_cast<void>(sync.apply(std::vector{ringDraw(7, 3)}, 1));
    CHECK(fixture.references(frame3) == 1);
    // The ring ages into the next baked frame: same key, rebound mesh.
    pa::PresentationSyncReport report = sync.apply(std::vector{ringDraw(7, 4)}, 2);
    CHECK(report.rebound == 1);
    CHECK(fixture.references(frame3) == 0);
    CHECK(fixture.references(frame4) == 1);
    // Effect expired.
    report = sync.apply(std::vector<r::PresentationDraw>{}, 3);
    CHECK(report.removed == 1);
    CHECK(fixture.references(frame4) == 0);
    CHECK(sync.objectCount() == 0);
    // Shared meshes stay resident for the next placement; eviction is an
    // explicit owner decision, not part of the effect lifecycle.
    CHECK(fixture.store.contains(pa::GpuMeshKey::presentation(frame4)));
}

void badFramesLeaveTheSceneUntouched() {
    Fixture fixture;
    pa::PresentationObjectSync sync(fixture.registry, fixture.store);
    static_cast<void>(sync.apply(std::vector{residentDraw(1, {0, 0, 0})}, 1));

    r::PresentationDraw unknownMesh = residentDraw(2, {0, 0, 0});
    unknownMesh.mesh = {r::PresentationMeshKind::SpeechGlyph, 'A'};
    CHECK_THROWS(std::out_of_range,
                 static_cast<void>(sync.apply(std::vector{unknownMesh}, 2)));
    CHECK_THROWS(std::invalid_argument,
                 static_cast<void>(sync.apply(
                     std::vector{residentDraw(3, {0, 0, 0}), residentDraw(3, {1, 0, 0})}, 2)));
    r::PresentationDraw nan = residentDraw(4, {0, 0, 0});
    nan.model[12] = std::numeric_limits<float>::quiet_NaN();
    CHECK_THROWS(std::invalid_argument, static_cast<void>(sync.apply(std::vector{nan}, 2)));
    CHECK_THROWS(std::invalid_argument, sync.validate(std::vector{nan}));
    // Resident 1 survived every rejected frame.
    CHECK(sync.objectCount() == 1);
    CHECK(fixture.registry.total_object_count() == 1);
    CHECK(fixture.references(kResidentMesh) == 1);
}

void residentsShareOneInstancedBatch() {
    Fixture fixture;
    pa::PictorSceneSync facilities(fixture.registry, fixture.store);
    pa::PresentationObjectSync sync(fixture.registry, fixture.store);
    std::vector<r::PresentationDraw> draws;
    for (std::uint64_t owner = 1; owner <= 5; ++owner) {
        draws.push_back(residentDraw(owner, {static_cast<double>(owner), 0, 0}));
    }
    draws.push_back(ringDraw(9, 2));
    static_cast<void>(sync.apply(draws, 1));

    // The plan asks every owner for tints, as the frame bridge does.
    struct Combined final : pa::IObjectTintSource {
        const pa::IObjectTintSource* a;
        const pa::IObjectTintSource* b;
        const r::WorldColor* tintFor(const ::pictor::ObjectId object) const noexcept override {
            const r::WorldColor* tint = a->tintFor(object);
            return tint != nullptr ? tint : b->tintFor(object);
        }
    } combined;
    combined.a = &facilities;
    combined.b = &sync;

    ::pictor::CullingSystem culling{fixture.registry};
    ::pictor::BatchBuilder batches{fixture.registry};
    pa::PictorBatchPlan plan;
    fixture.memory.begin_frame();
    culling.cull(pa::frustumFromViewProjection(wideOrtho()), fixture.memory.frame_allocator());
    batches.build(fixture.memory.frame_allocator());
    pa::buildPictorBatchPlan(fixture.registry, batches, combined, plan);

    CHECK(plan.opaque.size() == 1);
    CHECK(plan.translucent.size() == 1);
    CHECK(plan.instances.size() == 6);
    if (plan.opaque.size() == 1) {
        const ::pictor::RenderBatch& residents = plan.opaque[0];
        CHECK(residents.count == 5);
        CHECK(residents.mesh == *fixture.store.find(pa::GpuMeshKey::presentation(kResidentMesh)));
        std::vector<float> xs;
        for (std::uint32_t i = 0; i < residents.count; ++i) {
            xs.push_back(plan.instances[residents.startIndex + i].model[12]);
        }
        std::sort(xs.begin(), xs.end());
        CHECK(xs == (std::vector<float>{1, 2, 3, 4, 5}));
    }
    if (plan.translucent.size() == 1) {
        CHECK(plan.translucent[0].count == 1);
        CHECK(plan.translucent[0].transparency == 1);
    }
}

void frameBridgeSyncsPresentationWithFacilities() {
    Fixture fixture;
    pa::PictorFrameBridge bridge;
    bridge.initialize(fixture.store, 2);

    r::WorldDrawList drawList;
    drawList.snapshotTick = 1;
    drawList.presentation = {residentDraw(1, {0, 0, 0}), residentDraw(2, {2, 0, 0}),
                             ringDraw(4, 0)};
    static_cast<void>(bridge.consume(drawList, wideOrtho()));
    CHECK(bridge.presentationObjectCount() == 3);
    CHECK(bridge.stats().objects == 3);
    CHECK(bridge.stats().presentationObjects == 3);
    CHECK(bridge.lastPresentationSync().added == 3);
    CHECK(fixture.references(kResidentMesh) == 2);

    // A frame with an unknown mesh fails before anything changes.
    r::WorldDrawList bad = drawList;
    bad.presentation.push_back(residentDraw(3, {0, 0, 0}));
    bad.presentation.back().mesh = {r::PresentationMeshKind::SpeechGlyph, 'Z'};
    CHECK_THROWS(std::out_of_range, static_cast<void>(bridge.consume(bad, wideOrtho())));
    CHECK(bridge.presentationObjectCount() == 3);

    // Resident 2 removed, effect expired.
    drawList.presentation = {residentDraw(1, {0.5, 0, 0})};
    static_cast<void>(bridge.consume(drawList, wideOrtho()));
    CHECK(bridge.presentationObjectCount() == 1);
    CHECK(bridge.lastPresentationSync().removed == 2);
    CHECK(bridge.lastPresentationSync().moved == 1);
    CHECK(fixture.references(kResidentMesh) == 1);

    bridge.shutdown();
    CHECK(fixture.references(kResidentMesh) == 0);
    CHECK(fixture.registry.total_object_count() == 0);
}

}  // namespace

int main() {
    presentationMeshesUploadOnceInTheirOwnKeySpace();
    residentObjectsFollowTheStableIdLifecycle();
    expiredEffectsReleaseTheirFrameMesh();
    badFramesLeaveTheSceneUntouched();
    residentsShareOneInstancedBatch();
    frameBridgeSyncsPresentationWithFacilities();
    return konbini::test::summarize("konbini_pictor_presentation_tests");
}
