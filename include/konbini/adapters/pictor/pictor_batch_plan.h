#pragma once

#include <array>
#include <cstdint>
#include <vector>

#ifndef NOGDI
#define NOGDI
#endif
#include "pictor/batch/batch_builder.h"
#include "pictor/scene/scene_registry.h"

// @implements spec/interface/pictor-rendering.md Required bridge

namespace konbini::adapters::pictor {

class IObjectTintSource;

// Per-instance data read by `konbini_world_instanced.vert` as
// `instances[gl_InstanceIndex]` (std430: mat4 model, vec4 tint).
// `model` is column-major like Pictor's `float4x4` and GLSL's `mat4`.
struct WorldInstanceRecord {
    std::array<float, 16> model{};
    std::array<float, 4> tint{1.0F, 1.0F, 1.0F, 1.0F};
};

static_assert(sizeof(WorldInstanceRecord) == sizeof(float) * 20);

// One frame's draw plan derived from Pictor's batches.
//
// `CompiledBatchRecorder` draws every batch with
// `firstInstance = batch.startIndex`, an index into the DYNAMIC pool's sorted
// order. `instances[i]` is therefore the record of the object at sorted
// position `i`, which makes `gl_InstanceIndex` address the right object for
// both single and instanced batches.
//
// The plan splits batches by shader key into the opaque and translucent lists
// the two world passes hand to the recorder, and requires each key to match
// Pictor's transparency category (`RenderBatch::transparency`), which the
// recorder's `filter_mask` uses for the same split.
struct PictorBatchPlan {
    std::vector<::pictor::RenderBatch> opaque;
    std::vector<::pictor::RenderBatch> translucent;
    std::vector<WorldInstanceRecord> instances;
};

// Rebuilds `plan` (keeping its capacity) after `batches.build()` ran for the
// current frame. Objects outside the DYNAMIC pool, unknown shader keys, batch
// ranges outside the sorted indices, a missing sorted index array, and
// objects without a sync mapping are `std::logic_error` / `std::runtime_error`;
// none of them is drawn as a guess. `tints` answers for every registered
// object (facility and presentation syncs).
void buildPictorBatchPlan(
    const ::pictor::SceneRegistry& registry,
    const ::pictor::BatchBuilder& batches, const IObjectTintSource& tints,
    PictorBatchPlan& plan);

}  // namespace konbini::adapters::pictor
