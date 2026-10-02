#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

#include "konbini/render/world_draw_list.h"
#include "konbini/render/world_palette.h"
#include "konbini/sim/entity_id.h"
#include "konbini/sim/figmentum_facility_key.h"

#ifndef NOGDI
#define NOGDI
#endif
#include "pictor/scene/scene_registry.h"

// @implements spec/interface/pictor-rendering.md Object lifecycle

namespace konbini::adapters::pictor {

class GpuAssetStore;

// One facility the current snapshot wants drawn through Pictor.
struct SceneObjectRequest {
    sim::FacilityId facility{};
    sim::FigmentumFacilityKey asset{};
    render::WorldColor tint{1.0F, 1.0F, 1.0F, 1.0F};
    // alpha < 1 facilities (BASE-FP-DESTROYED-01) draw in the depth-read-only
    // translucent pass.
    bool translucent = false;
};

// Projects the facility draws of a `WorldDrawList` into requests, base
// facilities first. The store / overlay meshes are rebuilt every frame and
// stay on the per-flight buffer path, so they are not Pictor objects.
[[nodiscard]] std::vector<SceneObjectRequest> sceneObjectRequests(
    const render::WorldDrawList& drawList);

struct SceneSyncReport {
    std::uint32_t added = 0;
    std::uint32_t removed = 0;
    // Same facility, different mesh or pass: unregistered and re-registered.
    std::uint32_t rebound = 0;
    // Same facility and mesh, only the per-instance tint changed.
    std::uint32_t retinted = 0;
};

// Keeps `FacilityId → Pictor ObjectId` in step with the snapshot
// (pictor-rendering.md#Object lifecycle).
//
// `apply()` diffs the full request set against the registered objects:
// Added → register after resolving the mesh, Removed → unregister and drop the
// mapping, changed mesh / pass → re-register, tint → per-instance data only.
// A facility replaced by a store, or a whole dimension leaving the view,
// therefore disappears from the registry in the same frame, and the registry
// never holds an object without a mapping (checked after every apply).
//
// Objects go to Pictor's DYNAMIC pool (BASE-GATE5-POOL-01): pinned Pictor's
// recorder draws with `firstInstance = batch.startIndex`, which only maps back
// to an object through `BatchBuilder::sorted_indices()`, and that array
// exists for the DYNAMIC pool only.
//
// Each object's `materialKey` is its mesh handle. `BatchBuilder` merges
// consecutive objects with equal shader + material keys and draws the merged
// run with the first object's mesh, so the material key must separate
// meshes; objects that share a mesh become one instanced draw.
//
// `registry` and `assets` are borrowed and must outlive this sync.
class PictorSceneSync {
public:
    PictorSceneSync(::pictor::SceneRegistry& registry, GpuAssetStore& assets);
    ~PictorSceneSync();

    PictorSceneSync(const PictorSceneSync&) = delete;
    PictorSceneSync& operator=(const PictorSceneSync&) = delete;

    // Every request is validated before the registry changes: invalid IDs,
    // duplicate facilities and non-finite tints are `std::invalid_argument`,
    // a facility whose mesh is not in the asset store is `std::out_of_range`.
    // `frameSerial` dates the asset releases for deferred eviction.
    SceneSyncReport apply(
        std::span<const SceneObjectRequest> requests,
        std::uint64_t frameSerial);

    // Unregisters every object and releases its mesh reference.
    void clear(std::uint64_t frameSerial) noexcept;

    [[nodiscard]] std::size_t objectCount() const noexcept;
    [[nodiscard]] std::optional<::pictor::ObjectId> objectFor(
        sim::FacilityId facility) const noexcept;
    // Per-instance tint of a registered object, nullptr when unknown.
    [[nodiscard]] const render::WorldColor* tintFor(
        ::pictor::ObjectId object) const noexcept;

private:
    struct Binding {
        ::pictor::ObjectId object = ::pictor::INVALID_OBJECT_ID;
        ::pictor::MeshHandle mesh = ::pictor::INVALID_MESH;
        bool translucent = false;
    };

    void registerObject(sim::FacilityId facility, ::pictor::MeshHandle mesh,
                        const SceneObjectRequest& request);
    void unregisterObject(
        std::map<sim::FacilityId, Binding>::iterator binding,
        std::uint64_t frameSerial);

    ::pictor::SceneRegistry* registry_ = nullptr;
    GpuAssetStore* assets_ = nullptr;
    std::map<sim::FacilityId, Binding> bindings_;
    std::unordered_map<::pictor::ObjectId, render::WorldColor> tints_;
};

}  // namespace konbini::adapters::pictor
