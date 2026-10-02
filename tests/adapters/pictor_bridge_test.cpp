#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

#include "../check.h"

#include "konbini/adapters/pictor/gpu_asset_store.h"
#include "konbini/adapters/pictor/konbini_batch_gpu_source.h"
#include "konbini/adapters/pictor/pictor_batch_plan.h"
#include "konbini/adapters/pictor/pictor_scene_sync.h"
#include "konbini/adapters/pictor/world_frustum.h"
#include "konbini/adapters/pictor/world_shader_keys.h"
#include "pictor/batch/batch_builder.h"
#include "pictor/culling/culling_system.h"
#include "pictor/memory/memory_subsystem.h"
#include "pictor/scene/scene_registry.h"

// Gate 5 production bridge without a Vulkan device: GPU asset lifetime,
// IBatchGpuSource resolution, FacilityId <-> ObjectId diffing, and the
// instance path through Pictor's real SceneRegistry / CullingSystem /
// BatchBuilder. Buffer and pipeline handles are opaque fakes compared by
// identity; nothing here dereferences them. The real Vulkan upload, record
// and present path is not covered by this binary
// (verification-strategy.md#5. Pictor / Ergo integration).
//
// @implements spec/test/verification-strategy.md 5. Pictor / Ergo integration
// @implements spec/interface/pictor-rendering.md Required bridge

namespace {

namespace pa = konbini::adapters::pictor;
using konbini::render::WorldColor;
using konbini::render::WorldMesh;
using konbini::render::WorldVertex;
using konbini::sim::FacilityId;
using konbini::sim::FigmentumFacilityKey;

template <typename Handle>
Handle fakeHandle(const std::uintptr_t value) {
    return reinterpret_cast<Handle>(value);
}

// Records every upload / release so tests can see leaks and release order.
class FakeUploader final : public pa::IGpuMeshUploader {
public:
    pa::GpuMeshBuffers upload(const WorldMesh& mesh) override {
        if (failNext) {
            failNext = false;
            throw std::runtime_error("fake upload failure");
        }
        ++nextId_;
        const pa::GpuMeshBuffers buffers{
            .vertexBuffer = fakeHandle<VkBuffer>(0x10000 + nextId_ * 2),
            .indexBuffer = fakeHandle<VkBuffer>(0x10001 + nextId_ * 2),
            .indexCount = static_cast<std::uint32_t>(mesh.indices.size()),
            .residentBytes = mesh.vertices.size() * sizeof(WorldVertex) +
                             mesh.indices.size() * sizeof(std::uint32_t),
        };
        live.push_back(buffers.vertexBuffer);
        ++uploads;
        return buffers;
    }

    void release(const pa::GpuMeshBuffers& buffers) noexcept override {
        released.push_back(buffers.vertexBuffer);
        live.erase(std::remove(live.begin(), live.end(), buffers.vertexBuffer),
                   live.end());
    }

    std::vector<VkBuffer> live;
    std::vector<VkBuffer> released;
    int uploads = 0;
    bool failNext = false;

private:
    std::uintptr_t nextId_ = 0;
};

WorldMesh triangleAt(const float x, const float y, const float z) {
    WorldMesh mesh;
    mesh.vertices = {
        WorldVertex{.position = {x, y, z}},
        WorldVertex{.position = {x + 1.0F, y, z}},
        WorldVertex{.position = {x, y + 2.0F, z + 3.0F}},
    };
    mesh.indices = {0, 1, 2};
    return mesh;
}

FigmentumFacilityKey key(const std::uint64_t value) {
    return FigmentumFacilityKey{value};
}

FacilityId facility(const std::uint32_t index) {
    return FacilityId{{index, 1}};
}

constexpr WorldColor kRed{1.0F, 0.0F, 0.0F, 1.0F};
constexpr WorldColor kGreen{0.0F, 1.0F, 0.0F, 1.0F};
constexpr WorldColor kGhost{0.5F, 0.5F, 0.5F, 0.5F};

::pictor::MemoryConfig smallMemory() {
    ::pictor::MemoryConfig config;
    config.frame_allocator_size = 1024U * 1024U;
    config.flight_count = 2;
    return config;
}

// Column-major orthographic matrix that maps |x|,|y| <= 100 to clip [-1, 1]
// and z in [-100, 100] to depth [0, 1].
std::array<float, 16> wideOrtho() {
    std::array<float, 16> m{};
    m[0 * 4 + 0] = 0.01F;
    m[1 * 4 + 1] = 0.01F;
    m[2 * 4 + 2] = 0.005F;
    m[3 * 4 + 2] = 0.5F;
    m[3 * 4 + 3] = 1.0F;
    return m;
}

// --- GpuAssetStore -------------------------------------------------------

void assetStoreValidatesBeforeUploading() {
    FakeUploader uploader;
    pa::GpuAssetStore store;
    CHECK_THROWS(std::invalid_argument, store.initialize(uploader, 0));
    store.initialize(uploader, 2);

    CHECK_THROWS(std::invalid_argument,
                 static_cast<void>(store.insert(key(0), triangleAt(0, 0, 0))));
    CHECK_THROWS(std::invalid_argument,
                 static_cast<void>(store.insert(key(1), WorldMesh{})));
    WorldMesh notTriangles = triangleAt(0, 0, 0);
    notTriangles.indices.push_back(0);
    CHECK_THROWS(std::invalid_argument,
                 static_cast<void>(store.insert(key(1), notTriangles)));
    WorldMesh outOfRange = triangleAt(0, 0, 0);
    outOfRange.indices[2] = 3;
    CHECK_THROWS(std::invalid_argument,
                 static_cast<void>(store.insert(key(1), outOfRange)));
    WorldMesh nonFinite = triangleAt(0, 0, 0);
    nonFinite.vertices[1].position[0] =
        std::numeric_limits<float>::quiet_NaN();
    CHECK_THROWS(std::invalid_argument,
                 static_cast<void>(store.insert(key(1), nonFinite)));
    // Rejected meshes never reached the GPU.
    CHECK(uploader.uploads == 0);

    uploader.failNext = true;
    CHECK_THROWS(std::runtime_error,
                 static_cast<void>(store.insert(key(1), triangleAt(0, 0, 0))));
    CHECK(!store.contains(key(1)));
    CHECK(store.size() == 0);

    const ::pictor::MeshHandle first = store.insert(key(1), triangleAt(2, 3, 4));
    CHECK(store.contains(key(1)));
    CHECK(store.find(key(1)) == first);
    CHECK_THROWS(std::logic_error,
                 static_cast<void>(store.insert(key(1), triangleAt(0, 0, 0))));

    const pa::GpuMeshAsset* const asset = store.resolve(first);
    CHECK(asset != nullptr);
    CHECK(asset->buffers.indexCount == 3);
    CHECK(asset->bounds.min.x == 2.0F && asset->bounds.min.y == 3.0F &&
          asset->bounds.min.z == 4.0F);
    CHECK(asset->bounds.max.x == 3.0F && asset->bounds.max.y == 5.0F &&
          asset->bounds.max.z == 7.0F);
}

void assetStoreCountsReferencesAndEvictsAfterLatency() {
    FakeUploader uploader;
    pa::GpuAssetStore store;
    store.initialize(uploader, 2);
    const ::pictor::MeshHandle mesh = store.insert(key(7), triangleAt(0, 0, 0));

    CHECK_THROWS(std::out_of_range, store.acquire(mesh + 100));
    CHECK_THROWS(std::logic_error, store.release(mesh, 0));

    store.acquire(mesh);
    store.acquire(mesh);
    CHECK(store.stats().referencedAssets == 1);
    // Referenced meshes are never evicted.
    CHECK(store.evictUnreferenced(1000) == 0);

    store.release(mesh, 10);
    CHECK(store.evictUnreferenced(1000) == 0);
    store.release(mesh, 10);
    CHECK(store.stats().referencedAssets == 0);
    // Flights 10 and 11 may still read the mesh.
    CHECK(store.evictUnreferenced(10) == 0);
    CHECK(store.evictUnreferenced(11) == 0);
    CHECK(store.evictUnreferenced(12) == 1);
    CHECK(store.resolve(mesh) == nullptr);
    CHECK(!store.contains(key(7)));
    CHECK(uploader.live.empty());

    // A re-upload of the same key gets a fresh handle; the stale one stays
    // unresolvable instead of aliasing the new mesh.
    const ::pictor::MeshHandle again = store.insert(key(7), triangleAt(0, 0, 0));
    CHECK(again != mesh);
    CHECK(store.resolve(mesh) == nullptr);
    CHECK(store.resolve(again) != nullptr);
}

void assetStoreRepeatedCreateDestroyLeavesNoResource() {
    FakeUploader uploader;
    pa::GpuAssetStore store;
    store.initialize(uploader, 3);
    std::uint64_t frame = 0;
    for (std::uint64_t cycle = 1; cycle <= 200; ++cycle) {
        const ::pictor::MeshHandle mesh =
            store.insert(key(cycle), triangleAt(0, 0, 0));
        store.acquire(mesh);
        store.release(mesh, ++frame);
        frame += 3;
        CHECK(store.evictUnreferenced(frame) == 1);
    }
    CHECK(uploader.live.empty());
    CHECK(store.size() == 0);
    CHECK(store.stats().uploads == 200);
    CHECK(store.stats().evictions == 200);
    CHECK(store.stats().residentBytes == 0);
}

void assetStoreShutdownReleasesInReverseOrder() {
    FakeUploader uploader;
    {
        pa::GpuAssetStore store;
        store.initialize(uploader, 2);
        static_cast<void>(store.insert(key(1), triangleAt(0, 0, 0)));
        static_cast<void>(store.insert(key(2), triangleAt(0, 0, 0)));
        static_cast<void>(store.insert(key(3), triangleAt(0, 0, 0)));
        const std::vector<VkBuffer> inserted = uploader.live;
        store.shutdown();
        CHECK(!store.isInitialized());
        CHECK(uploader.released.size() == 3);
        CHECK(uploader.released ==
              std::vector<VkBuffer>(inserted.rbegin(), inserted.rend()));
    }
    CHECK(uploader.live.empty());
}

// --- KonbiniBatchGpuSource -------------------------------------------------

void batchSourceResolvesAndRecordsFailures() {
    FakeUploader uploader;
    pa::GpuAssetStore store;
    store.initialize(uploader, 2);
    const ::pictor::MeshHandle mesh = store.insert(key(1), triangleAt(0, 0, 0));
    const pa::GpuMeshAsset& asset = *store.resolve(mesh);

    pa::KonbiniBatchGpuSource source(store);
    const VkPipeline opaque = fakeHandle<VkPipeline>(0x7000);
    const VkPipeline translucent = fakeHandle<VkPipeline>(0x7001);
    const std::array<pa::BatchPipelineBinding, 1> nullBinding{{
        {.shaderKey = pa::kOpaqueWorldShaderKey,
         .passType = ::pictor::PassType::OPAQUE,
         .pipeline = VK_NULL_HANDLE},
    }};
    CHECK_THROWS(std::invalid_argument, source.bindPipelines(nullBinding));
    const std::array<pa::BatchPipelineBinding, 2> duplicate{{
        {pa::kOpaqueWorldShaderKey, ::pictor::PassType::OPAQUE, opaque},
        {pa::kOpaqueWorldShaderKey, ::pictor::PassType::OPAQUE, translucent},
    }};
    CHECK_THROWS(std::invalid_argument, source.bindPipelines(duplicate));
    const std::array<pa::BatchPipelineBinding, 2> bindings{{
        {pa::kOpaqueWorldShaderKey, ::pictor::PassType::OPAQUE, opaque},
        {pa::kTranslucentWorldShaderKey, ::pictor::PassType::TRANSPARENT,
         translucent},
    }};
    source.bindPipelines(bindings);
    CHECK(source.pipelineCount() == 2);

    ::pictor::RenderBatch batch;
    batch.mesh = mesh;
    batch.count = 1;
    batch.shaderKey = pa::kOpaqueWorldShaderKey;
    ::pictor::BatchGpuResources resources;
    CHECK(source.resolve(batch, pa::kOpaqueWorldShaderKey,
                         ::pictor::PassType::OPAQUE, resources));
    CHECK(resources.pipeline == opaque);
    CHECK(resources.vertex_buffer == asset.buffers.vertexBuffer);
    CHECK(resources.index_buffer == asset.buffers.indexBuffer);
    CHECK(resources.index_type == VK_INDEX_TYPE_UINT32);
    CHECK(resources.index_count == 3);
    CHECK(source.failures().empty());

    // An opaque batch handed to the translucent pass has no pipeline there.
    CHECK(!source.resolve(batch, pa::kOpaqueWorldShaderKey,
                          ::pictor::PassType::TRANSPARENT, resources));
    CHECK(resources.pipeline == VK_NULL_HANDLE);
    CHECK(source.failures().size() == 1);
    CHECK(source.failures()[0].reason ==
          pa::BatchResolveFailureReason::MissingPipeline);

    batch.mesh = mesh + 50;
    CHECK(!source.resolve(batch, pa::kOpaqueWorldShaderKey,
                          ::pictor::PassType::OPAQUE, resources));
    CHECK(source.failures().size() == 2);
    CHECK(source.failures()[1].reason ==
          pa::BatchResolveFailureReason::UnknownMesh);
    CHECK(!pa::describeBatchResolveFailure(source.failures()[1]).empty());

    source.clearFailures();
    CHECK(source.failures().empty());
    source.clearPipelines();
    batch.mesh = mesh;
    CHECK(!source.resolve(batch, pa::kOpaqueWorldShaderKey,
                          ::pictor::PassType::OPAQUE, resources));
}

// --- PictorSceneSync -------------------------------------------------------

struct SceneFixture {
    FakeUploader uploader;
    pa::GpuAssetStore store;
    ::pictor::MemorySubsystem memory{smallMemory()};
    ::pictor::SceneRegistry registry{memory};
    ::pictor::MeshHandle meshA = ::pictor::INVALID_MESH;
    ::pictor::MeshHandle meshB = ::pictor::INVALID_MESH;

    SceneFixture() {
        store.initialize(uploader, 2);
        meshA = store.insert(key(100), triangleAt(0, 0, 0));
        meshB = store.insert(key(200), triangleAt(10, 0, 0));
    }

    [[nodiscard]] std::uint32_t references(const ::pictor::MeshHandle mesh) const {
        return store.resolve(mesh)->references;
    }
};

pa::SceneObjectRequest request(
    const std::uint32_t index, const std::uint64_t asset,
    const WorldColor& tint = kRed, const bool translucent = false) {
    return {.facility = facility(index),
            .asset = key(asset),
            .tint = tint,
            .translucent = translucent};
}

void sceneSyncDiffsAgainstTheSnapshot() {
    SceneFixture fixture;
    pa::PictorSceneSync sync(fixture.registry, fixture.store);

    const std::vector<pa::SceneObjectRequest> initial{
        request(1, 100), request(2, 100), request(3, 200)};
    pa::SceneSyncReport report = sync.apply(initial, 1);
    CHECK(report.added == 3 && report.removed == 0 && report.rebound == 0);
    CHECK(sync.objectCount() == 3);
    CHECK(fixture.registry.total_object_count() == 3);
    CHECK(fixture.registry.dynamic_pool().count() == 3);
    CHECK(fixture.references(fixture.meshA) == 2);
    CHECK(fixture.references(fixture.meshB) == 1);

    // Same snapshot again: nothing to do.
    report = sync.apply(initial, 2);
    CHECK(report.added == 0 && report.removed == 0 && report.rebound == 0 &&
          report.retinted == 0);

    // Tint only: per-instance data, the Pictor object stays.
    const ::pictor::ObjectId before = *sync.objectFor(facility(1));
    std::vector<pa::SceneObjectRequest> next = initial;
    next[0].tint = kGreen;
    report = sync.apply(next, 3);
    CHECK(report.retinted == 1 && report.rebound == 0);
    CHECK(*sync.objectFor(facility(1)) == before);
    CHECK(*sync.tintFor(before) == kGreen);

    // Same frame: facility 2 is replaced by a store (leaves the draw list)
    // while facility 4 appears.
    next = {next[0], request(3, 200), request(4, 200)};
    report = sync.apply(next, 4);
    CHECK(report.added == 1 && report.removed == 1);
    CHECK(!sync.objectFor(facility(2)).has_value());
    CHECK(sync.objectFor(facility(4)).has_value());
    CHECK(fixture.registry.total_object_count() == 3);
    CHECK(fixture.references(fixture.meshA) == 1);
    CHECK(fixture.references(fixture.meshB) == 2);

    // Mesh or pass change re-registers.
    next[0].asset = key(200);
    next[1].translucent = true;
    next[1].tint = kGhost;
    report = sync.apply(next, 5);
    CHECK(report.rebound == 2);
    CHECK(*sync.objectFor(facility(1)) != before);
    CHECK(fixture.references(fixture.meshA) == 0);
    CHECK(fixture.references(fixture.meshB) == 3);
    const ::pictor::SceneRegistry::ObjectLocation location =
        fixture.registry.find_object(*sync.objectFor(facility(3)));
    CHECK(location.valid);
    CHECK((fixture.registry.dynamic_pool().flags()[location.pool_index] &
           ::pictor::ObjectFlags::TRANSPARENT) != 0);
    CHECK(fixture.registry.dynamic_pool().shader_keys()[location.pool_index] ==
          pa::kTranslucentWorldShaderKey);

    // Bulk unregister (e.g. the visible dimension collapsed).
    report = sync.apply({}, 6);
    CHECK(report.removed == 3);
    CHECK(sync.objectCount() == 0);
    CHECK(fixture.registry.total_object_count() == 0);
    CHECK(fixture.references(fixture.meshB) == 0);
    CHECK(fixture.store.resolve(fixture.meshB)->unreferencedSinceFrame == 6);
}

void sceneSyncRejectsBadSnapshotsWithoutChangingTheScene() {
    SceneFixture fixture;
    pa::PictorSceneSync sync(fixture.registry, fixture.store);
    const std::vector<pa::SceneObjectRequest> initial{request(1, 100)};
    static_cast<void>(sync.apply(initial, 1));

    const std::vector<pa::SceneObjectRequest> missingMesh{
        request(2, 100), request(3, 999)};
    CHECK_THROWS(std::out_of_range, sync.apply(missingMesh, 2));
    const std::vector<pa::SceneObjectRequest> duplicate{
        request(2, 100), request(2, 200)};
    CHECK_THROWS(std::invalid_argument, sync.apply(duplicate, 2));
    const std::vector<pa::SceneObjectRequest> invalidId{
        {.facility = FacilityId{}, .asset = key(100)}};
    CHECK_THROWS(std::invalid_argument, sync.apply(invalidId, 2));
    std::vector<pa::SceneObjectRequest> nanTint{request(2, 100)};
    nanTint[0].tint[1] = std::numeric_limits<float>::infinity();
    CHECK_THROWS(std::invalid_argument, sync.apply(nanTint, 2));

    CHECK(sync.objectCount() == 1);
    CHECK(sync.objectFor(facility(1)).has_value());
    CHECK(fixture.registry.total_object_count() == 1);
    CHECK(fixture.references(fixture.meshA) == 1);

    sync.clear(3);
    CHECK(fixture.registry.total_object_count() == 0);
    CHECK(fixture.references(fixture.meshA) == 0);
}

void sceneRequestsComeFromTheDrawList() {
    konbini::render::WorldDrawList drawList;
    drawList.baseFacilities = {
        {.figmentumKey = key(100), .facilityId = facility(1), .tint = kRed}};
    drawList.overlayFacilities = {
        {.figmentumKey = key(200), .facilityId = facility(2), .tint = kGhost}};
    const std::vector<pa::SceneObjectRequest> requests =
        pa::sceneObjectRequests(drawList);
    CHECK(requests.size() == 2);
    CHECK(requests[0].facility == facility(1) && !requests[0].translucent);
    CHECK(requests[1].facility == facility(2) && requests[1].translucent);
    CHECK(requests[1].tint == kGhost);
}

// --- frustum + batch plan (instance path) ---------------------------------

void frustumFollowsTheCameraConvention() {
    const ::pictor::Frustum frustum = pa::frustumFromViewProjection(wideOrtho());
    const ::pictor::AABB inside{{-1.0F, -1.0F, -1.0F}, {1.0F, 1.0F, 1.0F}};
    const ::pictor::AABB right{{150.0F, 0.0F, 0.0F}, {160.0F, 1.0F, 1.0F}};
    const ::pictor::AABB behind{{0.0F, 0.0F, -200.0F}, {1.0F, 1.0F, -150.0F}};
    const ::pictor::AABB straddling{{90.0F, 0.0F, 0.0F}, {110.0F, 1.0F, 1.0F}};
    CHECK(frustum.test_aabb(inside));
    CHECK(!frustum.test_aabb(right));
    CHECK(!frustum.test_aabb(behind));
    CHECK(frustum.test_aabb(straddling));

    std::array<float, 16> nonFinite = wideOrtho();
    nonFinite[5] = std::numeric_limits<float>::quiet_NaN();
    CHECK_THROWS(std::invalid_argument,
                 static_cast<void>(pa::frustumFromViewProjection(nonFinite)));
    CHECK_THROWS(std::invalid_argument,
                 static_cast<void>(pa::frustumFromViewProjection({})));
}

struct PlanFixture : SceneFixture {
    ::pictor::CullingSystem culling{registry};
    ::pictor::BatchBuilder batches{registry};
    pa::PictorBatchPlan plan;

    void build(const pa::PictorSceneSync& sync,
               const std::array<float, 16>& viewProjection) {
        memory.begin_frame();
        culling.cull(pa::frustumFromViewProjection(viewProjection),
                     memory.frame_allocator());
        batches.build(memory.frame_allocator());
        pa::buildPictorBatchPlan(registry, batches, sync, plan);
    }
};

void batchPlanSharesMeshesAsInstancesAndSplitsPasses() {
    PlanFixture fixture;
    pa::PictorSceneSync sync(fixture.registry, fixture.store);
    const std::vector<pa::SceneObjectRequest> requests{
        request(1, 100, kRed), request(2, 100, kGreen),
        request(3, 200, kGhost, true)};
    static_cast<void>(sync.apply(requests, 1));

    fixture.build(sync, wideOrtho());
    const pa::PictorBatchPlan& plan = fixture.plan;
    CHECK(plan.instances.size() == 3);
    CHECK(plan.opaque.size() == 1);
    CHECK(plan.translucent.size() == 1);
    if (plan.opaque.size() == 1 && plan.translucent.size() == 1) {
        // Facilities 1 and 2 share mesh A: one instanced draw of two.
        const ::pictor::RenderBatch& shared = plan.opaque[0];
        CHECK(shared.mesh == fixture.meshA);
        CHECK(shared.count == 2);
        const WorldColor first = plan.instances[shared.startIndex].tint;
        const WorldColor second = plan.instances[shared.startIndex + 1].tint;
        CHECK((first == kRed && second == kGreen) ||
              (first == kGreen && second == kRed));

        const ::pictor::RenderBatch& ghost = plan.translucent[0];
        CHECK(ghost.mesh == fixture.meshB);
        CHECK(ghost.count == 1);
        CHECK(ghost.transparency == 1);
        CHECK(shared.transparency == 0);
        CHECK(plan.instances[ghost.startIndex].tint == kGhost);
        // World-space facility geometry: identity model.
        const std::array<float, 16>& model =
            plan.instances[ghost.startIndex].model;
        CHECK(model[0] == 1.0F && model[5] == 1.0F && model[10] == 1.0F &&
              model[15] == 1.0F && model[12] == 0.0F);
    }

    // Distinct meshes with the same material never merge into one draw.
    static_cast<void>(sync.apply(
        std::vector<pa::SceneObjectRequest>{request(1, 100), request(2, 200)},
        2));
    fixture.build(sync, wideOrtho());
    CHECK(fixture.plan.opaque.size() == 2);
    CHECK(fixture.plan.translucent.empty());
}

void batchPlanDropsCulledObjects() {
    PlanFixture fixture;
    pa::PictorSceneSync sync(fixture.registry, fixture.store);
    static_cast<void>(sync.apply(
        std::vector<pa::SceneObjectRequest>{request(1, 100), request(2, 200)},
        1));
    // Shift the view so only mesh A (x 0..1) stays inside; mesh B sits at
    // x 10..11, beyond a 5 m half-width view centered at x = -4.
    std::array<float, 16> narrow{};
    narrow[0 * 4 + 0] = 0.2F;
    narrow[3 * 4 + 0] = 0.8F;
    narrow[1 * 4 + 1] = 0.01F;
    narrow[2 * 4 + 2] = 0.005F;
    narrow[3 * 4 + 2] = 0.5F;
    narrow[3 * 4 + 3] = 1.0F;
    fixture.build(sync, narrow);
    CHECK(fixture.culling.get_stats().visible_objects == 1);
    CHECK(fixture.plan.opaque.size() == 1);
    if (fixture.plan.opaque.size() == 1) {
        CHECK(fixture.plan.opaque[0].mesh == fixture.meshA);
    }
}

void batchPlanRejectsObjectsOutsideTheDynamicPool() {
    PlanFixture fixture;
    pa::PictorSceneSync sync(fixture.registry, fixture.store);
    ::pictor::ObjectDescriptor staticObject;
    staticObject.mesh = fixture.meshA;
    staticObject.flags = ::pictor::ObjectFlags::STATIC;
    staticObject.shaderKey = pa::kOpaqueWorldShaderKey;
    const ::pictor::ObjectId stray =
        fixture.registry.register_object(staticObject);
    CHECK_THROWS(std::logic_error, fixture.build(sync, wideOrtho()));
    fixture.registry.unregister_object(stray);

    // An object the sync does not know has no instance data to draw with.
    ::pictor::ObjectDescriptor unmapped = staticObject;
    unmapped.flags = ::pictor::ObjectFlags::DYNAMIC;
    static_cast<void>(fixture.registry.register_object(unmapped));
    CHECK_THROWS(std::logic_error, fixture.build(sync, wideOrtho()));
}

}  // namespace

int main() {
    assetStoreValidatesBeforeUploading();
    assetStoreCountsReferencesAndEvictsAfterLatency();
    assetStoreRepeatedCreateDestroyLeavesNoResource();
    assetStoreShutdownReleasesInReverseOrder();
    batchSourceResolvesAndRecordsFailures();
    sceneSyncDiffsAgainstTheSnapshot();
    sceneSyncRejectsBadSnapshotsWithoutChangingTheScene();
    sceneRequestsComeFromTheDrawList();
    frustumFollowsTheCameraConvention();
    batchPlanSharesMeshesAsInstancesAndSplitsPasses();
    batchPlanDropsCulledObjects();
    batchPlanRejectsObjectsOutsideTheDynamicPool();
    return konbini::test::summarize("konbini_pictor_bridge_tests");
}
