#include "konbini/adapters/pictor/gpu_asset_store.h"

#include <cmath>
#include <stdexcept>
#include <utility>

// @implements spec/interface/pictor-rendering.md Required bridge

namespace konbini::adapters::pictor {

namespace {

// Validates the mesh before any GPU allocation and returns its AABB.
// Non-finite positions would turn into non-finite bounds, which the Failure
// contract forbids passing on silently.
[[nodiscard]] ::pictor::AABB validatedBounds(const render::WorldMesh& mesh) {
    if (mesh.vertices.empty() || mesh.indices.empty()) {
        throw std::invalid_argument("GPU asset store rejects an empty mesh");
    }
    if (mesh.indices.size() % 3U != 0U) {
        throw std::invalid_argument(
            "GPU asset store requires a triangle list");
    }
    for (const std::uint32_t index : mesh.indices) {
        if (index >= mesh.vertices.size()) {
            throw std::invalid_argument(
                "GPU asset store mesh index is outside the vertex range");
        }
    }

    ::pictor::AABB bounds{};
    bool first = true;
    for (const render::WorldVertex& vertex : mesh.vertices) {
        for (const float component : vertex.position) {
            if (!std::isfinite(component)) {
                throw std::invalid_argument(
                    "GPU asset store mesh has a non-finite position");
            }
        }
        const ::pictor::float3 point{
            vertex.position[0], vertex.position[1], vertex.position[2]};
        if (first) {
            bounds.min = point;
            bounds.max = point;
            first = false;
            continue;
        }
        bounds = bounds.merge(::pictor::AABB{point, point});
    }
    return bounds;
}

}  // namespace

GpuAssetStore::~GpuAssetStore() {
    shutdown();
}

void GpuAssetStore::initialize(
    IGpuMeshUploader& uploader, const std::uint32_t retireLatencyFrames) {
    if (isInitialized()) {
        throw std::logic_error("GPU asset store is already initialized");
    }
    if (retireLatencyFrames == 0) {
        throw std::invalid_argument(
            "GPU asset store requires a non-zero retire latency");
    }
    uploader_ = &uploader;
    retireLatencyFrames_ = retireLatencyFrames;
    nextHandle_ = 0;
    uploads_ = 0;
    evictions_ = 0;
}

// @implements spec/interface/pictor-rendering.md Required bridge
void GpuAssetStore::shutdown() noexcept {
    if (uploader_ != nullptr) {
        // Reverse insertion order. Handles are monotonic, so the map's last
        // element is the newest upload.
        while (!assets_.empty()) {
            const auto newest = std::prev(assets_.end());
            uploader_->release(newest->second.buffers);
            assets_.erase(newest);
        }
    }
    assets_.clear();
    handleByKey_.clear();
    uploader_ = nullptr;
    retireLatencyFrames_ = 0;
}

bool GpuAssetStore::isInitialized() const noexcept {
    return uploader_ != nullptr;
}

std::uint32_t GpuAssetStore::retireLatencyFrames() const noexcept {
    return retireLatencyFrames_;
}

// @implements spec/interface/pictor-rendering.md Failure
::pictor::MeshHandle GpuAssetStore::insert(
    const sim::FigmentumFacilityKey key, const render::WorldMesh& mesh) {
    if (!isInitialized()) {
        throw std::logic_error("GPU asset store insert before initialization");
    }
    if (!key.isValid()) {
        throw std::invalid_argument(
            "GPU asset store rejects the reserved zero facility key");
    }
    if (handleByKey_.find(key) != handleByKey_.end()) {
        throw std::logic_error("GPU asset store already holds this key");
    }
    if (nextHandle_ == ::pictor::INVALID_MESH) {
        throw std::overflow_error("GPU asset store exhausted mesh handles");
    }
    const ::pictor::AABB bounds = validatedBounds(mesh);

    const GpuMeshBuffers buffers = uploader_->upload(mesh);
    if (buffers.vertexBuffer == VK_NULL_HANDLE ||
        buffers.indexBuffer == VK_NULL_HANDLE ||
        buffers.indexCount != mesh.indices.size()) {
        uploader_->release(buffers);
        throw std::runtime_error(
            "GPU mesh uploader returned an incomplete upload");
    }

    const ::pictor::MeshHandle handle = nextHandle_;
    try {
        assets_.emplace(handle, GpuMeshAsset{
                                    .key = key,
                                    .handle = handle,
                                    .buffers = buffers,
                                    .bounds = bounds,
                                });
        handleByKey_.emplace(key, handle);
    } catch (...) {
        // Never keep a GPU allocation that nothing can look up.
        assets_.erase(handle);
        uploader_->release(buffers);
        throw;
    }
    ++nextHandle_;
    ++uploads_;
    return handle;
}

bool GpuAssetStore::contains(
    const sim::FigmentumFacilityKey key) const noexcept {
    return handleByKey_.find(key) != handleByKey_.end();
}

std::optional<::pictor::MeshHandle> GpuAssetStore::find(
    const sim::FigmentumFacilityKey key) const noexcept {
    const auto entry = handleByKey_.find(key);
    if (entry == handleByKey_.end()) {
        return std::nullopt;
    }
    return entry->second;
}

const GpuMeshAsset* GpuAssetStore::resolve(
    const ::pictor::MeshHandle handle) const noexcept {
    const auto entry = assets_.find(handle);
    return entry == assets_.end() ? nullptr : &entry->second;
}

GpuMeshAsset& GpuAssetStore::requireAsset(const ::pictor::MeshHandle handle) {
    const auto entry = assets_.find(handle);
    if (entry == assets_.end()) {
        throw std::out_of_range("GPU asset store has no mesh for this handle");
    }
    return entry->second;
}

void GpuAssetStore::acquire(const ::pictor::MeshHandle handle) {
    GpuMeshAsset& asset = requireAsset(handle);
    if (asset.references == UINT32_MAX) {
        throw std::overflow_error("GPU asset reference count overflows");
    }
    ++asset.references;
}

void GpuAssetStore::release(
    const ::pictor::MeshHandle handle, const std::uint64_t frameSerial) {
    GpuMeshAsset& asset = requireAsset(handle);
    if (asset.references == 0) {
        throw std::logic_error("GPU asset released more often than acquired");
    }
    if (--asset.references == 0) {
        asset.unreferencedSinceFrame = frameSerial;
    }
}

// @implements spec/interface/pictor-rendering.md Required bridge
std::size_t GpuAssetStore::evictUnreferenced(
    const std::uint64_t currentFrameSerial) {
    if (!isInitialized()) {
        throw std::logic_error("GPU asset store evict before initialization");
    }
    std::size_t evicted = 0;
    // Newest first, consistent with shutdown.
    for (auto entry = assets_.end(); entry != assets_.begin();) {
        --entry;
        const GpuMeshAsset& asset = entry->second;
        if (asset.references != 0 ||
            currentFrameSerial <
                asset.unreferencedSinceFrame + retireLatencyFrames_) {
            continue;
        }
        handleByKey_.erase(asset.key);
        uploader_->release(asset.buffers);
        entry = assets_.erase(entry);
        ++evicted;
    }
    evictions_ += evicted;
    return evicted;
}

std::size_t GpuAssetStore::size() const noexcept {
    return assets_.size();
}

GpuAssetStoreStats GpuAssetStore::stats() const noexcept {
    GpuAssetStoreStats result{
        .residentAssets = assets_.size(),
        .uploads = uploads_,
        .evictions = evictions_,
    };
    for (const auto& [handle, asset] : assets_) {
        static_cast<void>(handle);
        result.residentBytes += asset.buffers.residentBytes;
        if (asset.references != 0) {
            ++result.referencedAssets;
        }
    }
    return result;
}

}  // namespace konbini::adapters::pictor
