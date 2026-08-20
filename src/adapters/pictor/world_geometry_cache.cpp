#include "konbini/adapters/pictor/world_geometry_cache.h"

#include <stdexcept>
#include <utility>

// @implements spec/interface/pictor-rendering.md Object lifecycle

namespace konbini::adapters::pictor {

WorldGeometryCache::~WorldGeometryCache() {
    shutdown();
}

void WorldGeometryCache::initialize(
    const VkPhysicalDevice physicalDevice, const VkDevice device) {
    if (isInitialized()) {
        throw std::logic_error("world geometry cache is already initialized");
    }
    if (physicalDevice == VK_NULL_HANDLE || device == VK_NULL_HANDLE) {
        throw std::invalid_argument(
            "world geometry cache requires a Vulkan device");
    }
    physicalDevice_ = physicalDevice;
    device_ = device;
}

void WorldGeometryCache::shutdown() noexcept {
    // insert 順の逆から解放する。Vulkan 上は独立した allocation だが、
    // 「確保と逆順で解放する」規律を cache 単位でも守る。
    while (!buffers_.empty()) {
        buffers_.back()->shutdown();
        buffers_.pop_back();
    }
    indexByKey_.clear();
    physicalDevice_ = VK_NULL_HANDLE;
    device_ = VK_NULL_HANDLE;
}

bool WorldGeometryCache::isInitialized() const noexcept {
    return device_ != VK_NULL_HANDLE;
}

// @implements spec/interface/pictor-rendering.md Object lifecycle
void WorldGeometryCache::insert(
    const sim::FigmentumFacilityKey key, const render::WorldMesh& mesh) {
    if (!isInitialized()) {
        throw std::logic_error(
            "world geometry cache insert requires initialization");
    }
    if (!key.isValid()) {
        throw std::invalid_argument(
            "world geometry cache rejects the reserved zero facility key");
    }
    if (mesh.vertices.empty() || mesh.indices.empty()) {
        throw std::invalid_argument(
            "world geometry cache rejects an empty facility mesh");
    }
    if (indexByKey_.find(key) != indexByKey_.end()) {
        throw std::logic_error(
            "world geometry cache already holds this facility key");
    }

    auto buffer = std::make_unique<WorldGeometryBuffer>();
    buffer->initialize(
        physicalDevice_, device_,
        WorldGeometryBuffer::requiredVertexBytes(mesh),
        WorldGeometryBuffer::requiredIndexBytes(mesh));
    buffer->upload(mesh);

    buffers_.push_back(std::move(buffer));
    try {
        indexByKey_.emplace(key, buffers_.size() - 1U);
    } catch (...) {
        // map への登録が失敗したら、引ける経路の無い buffer を残さない。
        buffers_.back()->shutdown();
        buffers_.pop_back();
        throw;
    }
}

bool WorldGeometryCache::contains(
    const sim::FigmentumFacilityKey key) const noexcept {
    return indexByKey_.find(key) != indexByKey_.end();
}

// @implements spec/interface/pictor-rendering.md Failure
const WorldGeometryBuffer& WorldGeometryCache::find(
    const sim::FigmentumFacilityKey key) const {
    const auto entry = indexByKey_.find(key);
    if (entry == indexByKey_.end()) {
        throw std::out_of_range(
            "world geometry cache has no geometry for this facility key");
    }
    return *buffers_[entry->second];
}

std::size_t WorldGeometryCache::size() const noexcept {
    return buffers_.size();
}

}  // namespace konbini::adapters::pictor
