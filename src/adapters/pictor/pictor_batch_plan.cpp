#include "konbini/adapters/pictor/pictor_batch_plan.h"

#include <cstring>
#include <stdexcept>

#include "konbini/adapters/pictor/pictor_scene_sync.h"
#include "konbini/adapters/pictor/world_shader_keys.h"

// @implements spec/interface/pictor-rendering.md Required bridge

namespace konbini::adapters::pictor {

// @implements spec/interface/pictor-rendering.md Pictor pools
void buildPictorBatchPlan(
    const ::pictor::SceneRegistry& registry,
    const ::pictor::BatchBuilder& batches, const PictorSceneSync& sync,
    PictorBatchPlan& plan) {
    plan.opaque.clear();
    plan.translucent.clear();
    plan.instances.clear();

    if (!registry.static_pool().empty() ||
        !registry.gpu_driven_pool().empty()) {
        // Their batches index arrays Pictor does not expose
        // (BASE-GATE5-POOL-01); drawing them would read the wrong instances.
        throw std::logic_error(
            "Pictor frame bridge only plans DYNAMIC pool objects");
    }

    const ::pictor::ObjectPool& pool = registry.dynamic_pool();
    const std::size_t sortedCount = batches.sorted_index_count();
    const std::uint32_t* const sorted = batches.sorted_indices();
    if (!batches.batches().empty() && sorted == nullptr) {
        // The frame allocator could not hold the sorted indices; the batches
        // then have no instance data to read.
        throw std::runtime_error(
            "Pictor batch builder produced batches without sorted indices");
    }

    for (const ::pictor::RenderBatch& batch : batches.batches()) {
        if (batch.count == 0 ||
            static_cast<std::size_t>(batch.startIndex) + batch.count >
                sortedCount) {
            throw std::logic_error(
                "Pictor batch range is outside the sorted instance indices");
        }
        if (batch.shaderKey == kOpaqueWorldShaderKey &&
            batch.transparency == 0) {
            plan.opaque.push_back(batch);
        } else if (batch.shaderKey == kTranslucentWorldShaderKey &&
                   batch.transparency != 0) {
            plan.translucent.push_back(batch);
        } else {
            // Either a key the bridge never assigns, or a shader key whose
            // pipeline disagrees with Pictor's transparency category.
            throw std::logic_error(
                "Pictor batch carries a shader key the bridge did not assign");
        }
    }

    plan.instances.resize(sortedCount);
    for (std::size_t position = 0; position < sortedCount; ++position) {
        const std::uint32_t poolIndex = sorted[position];
        if (poolIndex >= pool.count()) {
            throw std::logic_error(
                "Pictor sorted index is outside the DYNAMIC pool");
        }
        const ::pictor::ObjectId object = pool.object_ids()[poolIndex];
        const render::WorldColor* const tint = sync.tintFor(object);
        if (tint == nullptr) {
            throw std::logic_error(
                "Pictor object has no facility mapping in the scene sync");
        }
        WorldInstanceRecord& record = plan.instances[position];
        static_assert(sizeof(record.model) == sizeof(::pictor::float4x4));
        std::memcpy(record.model.data(), &pool.transforms()[poolIndex],
                    sizeof(record.model));
        record.tint = *tint;
    }
}

}  // namespace konbini::adapters::pictor
