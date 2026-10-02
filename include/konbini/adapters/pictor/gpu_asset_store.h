#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>

#include "konbini/adapters/pictor/gpu_mesh_key.h"
#include "konbini/adapters/pictor/gpu_mesh_uploader.h"
#include "konbini/render/world_mesh.h"
#include "konbini/sim/figmentum_facility_key.h"

// Pictor's core headers spell `ObjectFlags::TRANSPARENT` and
// `PassType::OPAQUE`, which collide with Win32 GDI macros.
#ifndef NOGDI
#define NOGDI
#endif
#include "pictor/core/types.h"

// @implements spec/interface/pictor-rendering.md Required bridge

namespace konbini::adapters::pictor {

// One resident mesh. `bounds` is the world-space AABB of the uploaded
// vertices; Pictor culls objects that use this mesh with it.
struct GpuMeshAsset {
    GpuMeshKey key{};
    ::pictor::MeshHandle handle = ::pictor::INVALID_MESH;
    GpuMeshBuffers buffers;
    ::pictor::AABB bounds{};
    std::uint32_t references = 0;
    // Frame serial at which `references` last dropped to zero (insert counts
    // as frame 0). Eviction waits `retireLatencyFrames` past this.
    std::uint64_t unreferencedSinceFrame = 0;
};

struct GpuAssetStoreStats {
    std::size_t residentAssets = 0;
    std::size_t referencedAssets = 0;
    VkDeviceSize residentBytes = 0;
    std::uint64_t uploads = 0;
    std::uint64_t evictions = 0;
};

// Game-owned GPU mesh store behind Pictor `MeshHandle`s
// (pictor-rendering.md#GpuAssetStore).
//
// - Owns the vertex / index buffers through the borrowed uploader.
// - Maps a `GpuMeshKey` (Figmentum facility key, or a shared presentation
//   mesh) to a `MeshHandle`. Handles are never
//   reused, so a stale handle resolves to nothing instead of another mesh.
// - The upload is complete when `insert()` returns, so every published handle
//   is resident: an object can only reference a mesh that is already
//   drawable, and "visible before upload" cannot be expressed.
// - Reference counts belong to the Pictor objects that draw the mesh.
//   `evictUnreferenced()` frees only meshes unreferenced for at least
//   `retireLatencyFrames` frames, i.e. after every flight that could still
//   record them has been fenced. Eviction is an explicit owner decision:
//   Figmentum geometry cannot be regenerated inside the frame loop, so the
//   first playable never evicts city meshes on its own.
// - `shutdown()` releases in reverse insertion order. The caller idles the
//   device first (device lost / shutdown).
class GpuAssetStore {
public:
    GpuAssetStore() = default;
    ~GpuAssetStore();

    GpuAssetStore(const GpuAssetStore&) = delete;
    GpuAssetStore& operator=(const GpuAssetStore&) = delete;

    // `uploader` is borrowed and must outlive the store.
    void initialize(IGpuMeshUploader& uploader, std::uint32_t retireLatencyFrames);
    void shutdown() noexcept;

    [[nodiscard]] bool isInitialized() const noexcept;
    [[nodiscard]] std::uint32_t retireLatencyFrames() const noexcept;

    // Validates and uploads `mesh`. A duplicate key is `std::logic_error`;
    // empty, non-triangle-list, out-of-range or non-finite meshes are
    // `std::invalid_argument`. Nothing is published when the upload throws.
    ::pictor::MeshHandle insert(GpuMeshKey key, const render::WorldMesh& mesh);
    ::pictor::MeshHandle insert(
        sim::FigmentumFacilityKey key, const render::WorldMesh& mesh);

    [[nodiscard]] bool contains(GpuMeshKey key) const noexcept;
    [[nodiscard]] bool contains(sim::FigmentumFacilityKey key) const noexcept;
    [[nodiscard]] std::optional<::pictor::MeshHandle> find(
        GpuMeshKey key) const noexcept;
    [[nodiscard]] std::optional<::pictor::MeshHandle> find(
        sim::FigmentumFacilityKey key) const noexcept;

    // nullptr for unknown or evicted handles; the batch source reports that
    // as an explicit missing resource.
    [[nodiscard]] const GpuMeshAsset* resolve(
        ::pictor::MeshHandle handle) const noexcept;

    // Unknown handles are `std::out_of_range`; releasing an unreferenced mesh
    // is `std::logic_error`.
    void acquire(::pictor::MeshHandle handle);
    void release(::pictor::MeshHandle handle, std::uint64_t frameSerial);

    // Frees meshes whose references stayed zero since a frame at least
    // `retireLatencyFrames` before `currentFrameSerial`. Returns the count.
    std::size_t evictUnreferenced(std::uint64_t currentFrameSerial);

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] GpuAssetStoreStats stats() const noexcept;

private:
    [[nodiscard]] GpuMeshAsset& requireAsset(::pictor::MeshHandle handle);

    IGpuMeshUploader* uploader_ = nullptr;
    std::uint32_t retireLatencyFrames_ = 0;
    ::pictor::MeshHandle nextHandle_ = 0;
    // Handles increase monotonically, so map order is insertion order.
    std::map<::pictor::MeshHandle, GpuMeshAsset> assets_;
    std::map<GpuMeshKey, ::pictor::MeshHandle> handleByKey_;
    std::uint64_t uploads_ = 0;
    std::uint64_t evictions_ = 0;
};

}  // namespace konbini::adapters::pictor
