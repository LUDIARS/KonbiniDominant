#include "konbini/adapters/pictor/pictor_scene_sync.h"

#include <cmath>
#include <stdexcept>
#include <utility>

#include "konbini/adapters/pictor/gpu_asset_store.h"
#include "konbini/adapters/pictor/world_shader_keys.h"

// @implements spec/interface/pictor-rendering.md Object lifecycle

namespace konbini::adapters::pictor {

namespace {

[[nodiscard]] bool isFinite(const render::WorldColor& color) noexcept {
    for (const float channel : color) {
        if (!std::isfinite(channel)) {
            return false;
        }
    }
    return true;
}

void appendRequests(
    const std::vector<render::WorldFacilityDraw>& draws, const bool translucent,
    std::vector<SceneObjectRequest>& out) {
    for (const render::WorldFacilityDraw& draw : draws) {
        out.push_back({
            .facility = draw.facilityId,
            .asset = draw.figmentumKey,
            .tint = draw.tint,
            .translucent = translucent,
        });
    }
}

}  // namespace

std::vector<SceneObjectRequest> sceneObjectRequests(
    const render::WorldDrawList& drawList) {
    std::vector<SceneObjectRequest> requests;
    requests.reserve(
        drawList.baseFacilities.size() + drawList.overlayFacilities.size());
    appendRequests(drawList.baseFacilities, false, requests);
    appendRequests(drawList.overlayFacilities, true, requests);
    return requests;
}

PictorSceneSync::PictorSceneSync(
    ::pictor::SceneRegistry& registry, GpuAssetStore& assets)
    : registry_(&registry), assets_(&assets) {}

PictorSceneSync::~PictorSceneSync() {
    clear(0);
}

// @implements spec/interface/pictor-rendering.md Object lifecycle
SceneSyncReport PictorSceneSync::apply(
    const std::span<const SceneObjectRequest> requests,
    const std::uint64_t frameSerial) {
    // Validate everything first so a bad snapshot leaves the scene untouched.
    std::map<sim::FacilityId, std::pair<const SceneObjectRequest*,
                                        ::pictor::MeshHandle>>
        wanted;
    for (const SceneObjectRequest& request : requests) {
        if (!request.facility.isValid() || !request.asset.isValid()) {
            throw std::invalid_argument(
                "Pictor scene sync received an invalid facility request");
        }
        if (!isFinite(request.tint)) {
            throw std::invalid_argument(
                "Pictor scene sync received a non-finite tint");
        }
        const std::optional<::pictor::MeshHandle> mesh =
            assets_->find(request.asset);
        if (!mesh.has_value()) {
            // Missing geometry is a generation / upload gap, never "nothing
            // to draw here".
            throw std::out_of_range(
                "Pictor scene sync has no resident mesh for a facility");
        }
        if (!wanted.emplace(request.facility, std::make_pair(&request, *mesh))
                 .second) {
            throw std::invalid_argument(
                "Pictor scene sync received a duplicate facility");
        }
    }

    SceneSyncReport report;
    for (auto binding = bindings_.begin(); binding != bindings_.end();) {
        const auto request = wanted.find(binding->first);
        if (request == wanted.end()) {
            const auto next = std::next(binding);
            unregisterObject(binding, frameSerial);
            binding = next;
            ++report.removed;
            continue;
        }
        const SceneObjectRequest& next = *request->second.first;
        const ::pictor::MeshHandle mesh = request->second.second;
        if (binding->second.mesh != mesh ||
            binding->second.translucent != next.translucent) {
            const sim::FacilityId facility = binding->first;
            const auto following = std::next(binding);
            unregisterObject(binding, frameSerial);
            registerObject(facility, mesh, next);
            binding = following;
            ++report.rebound;
            continue;
        }
        render::WorldColor& tint = tints_.at(binding->second.object);
        if (tint != next.tint) {
            tint = next.tint;
            ++report.retinted;
        }
        ++binding;
    }
    for (const auto& [facility, request] : wanted) {
        if (bindings_.find(facility) == bindings_.end()) {
            registerObject(facility, request.second, *request.first);
            ++report.added;
        }
    }

    // The registry also holds presentation objects, so the whole-registry
    // count is checked by the frame bridge; this sync checks its own maps.
    if (registry_->total_object_count() < bindings_.size() ||
        tints_.size() != bindings_.size()) {
        throw std::logic_error(
            "Pictor scene registry holds objects without a facility mapping");
    }
    return report;
}

void PictorSceneSync::registerObject(
    const sim::FacilityId facility, const ::pictor::MeshHandle mesh,
    const SceneObjectRequest& request) {
    const GpuMeshAsset* const asset = assets_->resolve(mesh);
    if (asset == nullptr) {
        throw std::out_of_range(
            "Pictor scene sync resolved a mesh that is no longer resident");
    }
    ::pictor::ObjectDescriptor descriptor;
    descriptor.mesh = mesh;
    // Facility geometry is generated in world space (1 unit = 1 m).
    descriptor.transform = ::pictor::float4x4::identity();
    descriptor.bounds = asset->bounds;
    descriptor.flags = ::pictor::ObjectFlags::DYNAMIC;
    if (request.translucent) {
        descriptor.flags = static_cast<std::uint16_t>(
            descriptor.flags | ::pictor::ObjectFlags::TRANSPARENT);
    }
    descriptor.shaderKey = request.translucent ? kTranslucentWorldShaderKey
                                               : kOpaqueWorldShaderKey;
    descriptor.materialKey = mesh;

    assets_->acquire(mesh);
    ::pictor::ObjectId object = ::pictor::INVALID_OBJECT_ID;
    try {
        object = registry_->register_object(descriptor);
        tints_.emplace(object, request.tint);
        bindings_.emplace(facility, Binding{
                                        .object = object,
                                        .mesh = mesh,
                                        .translucent = request.translucent,
                                    });
    } catch (...) {
        if (object != ::pictor::INVALID_OBJECT_ID) {
            tints_.erase(object);
            registry_->unregister_object(object);
        }
        assets_->release(mesh, 0);
        throw;
    }
}

void PictorSceneSync::unregisterObject(
    const std::map<sim::FacilityId, Binding>::iterator binding,
    const std::uint64_t frameSerial) {
    const Binding removed = binding->second;
    bindings_.erase(binding);
    tints_.erase(removed.object);
    registry_->unregister_object(removed.object);
    assets_->release(removed.mesh, frameSerial);
}

void PictorSceneSync::clear(const std::uint64_t frameSerial) noexcept {
    while (!bindings_.empty()) {
        const Binding removed = bindings_.begin()->second;
        bindings_.erase(bindings_.begin());
        registry_->unregister_object(removed.object);
        if (assets_->resolve(removed.mesh) != nullptr) {
            try {
                assets_->release(removed.mesh, frameSerial);
            } catch (...) {
                // Reference bookkeeping is already inconsistent; the store's
                // own shutdown still frees the buffers.
            }
        }
    }
    tints_.clear();
}

std::size_t PictorSceneSync::objectCount() const noexcept {
    return bindings_.size();
}

std::optional<::pictor::ObjectId> PictorSceneSync::objectFor(
    const sim::FacilityId facility) const noexcept {
    const auto binding = bindings_.find(facility);
    if (binding == bindings_.end()) {
        return std::nullopt;
    }
    return binding->second.object;
}

const render::WorldColor* PictorSceneSync::tintFor(
    const ::pictor::ObjectId object) const noexcept {
    const auto tint = tints_.find(object);
    return tint == tints_.end() ? nullptr : &tint->second;
}

}  // namespace konbini::adapters::pictor
