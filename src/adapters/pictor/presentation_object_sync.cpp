#include "konbini/adapters/pictor/presentation_object_sync.h"

#include <cmath>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>

#include "konbini/adapters/pictor/gpu_asset_store.h"
#include "konbini/adapters/pictor/gpu_mesh_key.h"
#include "konbini/adapters/pictor/world_shader_keys.h"

// @implements spec/interface/pictor-rendering.md Presentation objects

namespace konbini::adapters::pictor {

namespace {

template <typename Values>
[[nodiscard]] bool allFinite(const Values& values) noexcept {
    for (const float value : values) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] ::pictor::float4x4 toPictor(const std::array<float, 16>& model) {
    ::pictor::float4x4 matrix{};
    static_assert(sizeof(matrix.m) == sizeof(model));
    // Both are column-major: m[column][row] == model[column * 4 + row].
    std::memcpy(matrix.m, model.data(), sizeof(matrix.m));
    return matrix;
}

}  // namespace

PresentationObjectSync::PresentationObjectSync(
    ::pictor::SceneRegistry& registry, GpuAssetStore& assets)
    : registry_(&registry), assets_(&assets) {}

PresentationObjectSync::~PresentationObjectSync() {
    clear(0);
}

void PresentationObjectSync::validate(
    const std::span<const render::PresentationDraw> draws) const {
    static_cast<void>(collect(draws));
}

PresentationObjectSync::WantedMap PresentationObjectSync::collect(
    const std::span<const render::PresentationDraw> draws) const {
    WantedMap wanted;
    for (const render::PresentationDraw& draw : draws) {
        if (!allFinite(draw.model) || draw.model[15] != 1.0F ||
            !allFinite(draw.tint)) {
            throw std::invalid_argument(
                "presentation sync received a non-finite or projective draw");
        }
        const std::optional<::pictor::MeshHandle> mesh =
            assets_->find(GpuMeshKey::presentation(draw.mesh));
        if (!mesh.has_value()) {
            throw std::out_of_range(
                "presentation sync has no uploaded mesh for a draw");
        }
        if (!wanted.emplace(draw.key, std::make_pair(&draw, *mesh)).second) {
            throw std::invalid_argument(
                "presentation sync received a duplicate object key");
        }
    }
    return wanted;
}

// @implements spec/interface/pictor-rendering.md Presentation objects
PresentationSyncReport PresentationObjectSync::apply(
    const std::span<const render::PresentationDraw> draws,
    const std::uint64_t frameSerial) {
    // Validate everything first so a bad frame leaves the scene untouched.
    const WantedMap wanted = collect(draws);

    PresentationSyncReport report;
    for (auto binding = bindings_.begin(); binding != bindings_.end();) {
        const auto request = wanted.find(binding->first);
        if (request == wanted.end()) {
            const auto next = std::next(binding);
            unregisterObject(binding, frameSerial);
            binding = next;
            ++report.removed;
            continue;
        }
        const render::PresentationDraw& draw = *request->second.first;
        const ::pictor::MeshHandle mesh = request->second.second;
        if (binding->second.mesh != mesh ||
            binding->second.translucent != draw.translucent) {
            const render::PresentationObjectKey key = binding->first;
            const auto following = std::next(binding);
            unregisterObject(binding, frameSerial);
            registerObject(key, mesh, draw);
            binding = following;
            ++report.rebound;
            continue;
        }
        Binding& bound = binding->second;
        if (bound.model != draw.model) {
            bound.model = draw.model;
            registry_->update_transform(bound.object, toPictor(draw.model));
            registry_->update_bounds(bound.object,
                                     worldBounds(mesh, draw.model));
            ++report.moved;
        }
        render::WorldColor& tint = tints_.at(bound.object);
        if (tint != draw.tint) {
            tint = draw.tint;
            ++report.retinted;
        }
        ++binding;
    }
    for (const auto& [key, request] : wanted) {
        if (bindings_.find(key) == bindings_.end()) {
            registerObject(key, request.second, *request.first);
            ++report.added;
        }
    }

    if (tints_.size() != bindings_.size()) {
        throw std::logic_error(
            "presentation sync tint map diverged from its object map");
    }
    return report;
}

::pictor::AABB PresentationObjectSync::worldBounds(
    const ::pictor::MeshHandle mesh,
    const std::array<float, 16>& model) const {
    const GpuMeshAsset* const asset = assets_->resolve(mesh);
    if (asset == nullptr) {
        throw std::out_of_range(
            "presentation sync resolved a mesh that is no longer resident");
    }
    const ::pictor::AABB& local = asset->bounds;
    ::pictor::AABB world{};
    for (int corner = 0; corner < 8; ++corner) {
        const float x = (corner & 1) != 0 ? local.max.x : local.min.x;
        const float y = (corner & 2) != 0 ? local.max.y : local.min.y;
        const float z = (corner & 4) != 0 ? local.max.z : local.min.z;
        const ::pictor::float3 point{
            model[0] * x + model[4] * y + model[8] * z + model[12],
            model[1] * x + model[5] * y + model[9] * z + model[13],
            model[2] * x + model[6] * y + model[10] * z + model[14],
        };
        world = corner == 0 ? ::pictor::AABB{point, point}
                            : world.merge(::pictor::AABB{point, point});
    }
    return world;
}

void PresentationObjectSync::registerObject(
    const render::PresentationObjectKey& key, const ::pictor::MeshHandle mesh,
    const render::PresentationDraw& draw) {
    ::pictor::ObjectDescriptor descriptor;
    descriptor.mesh = mesh;
    descriptor.transform = toPictor(draw.model);
    descriptor.bounds = worldBounds(mesh, draw.model);
    descriptor.flags = ::pictor::ObjectFlags::DYNAMIC;
    if (draw.translucent) {
        descriptor.flags = static_cast<std::uint16_t>(
            descriptor.flags | ::pictor::ObjectFlags::TRANSPARENT);
    }
    descriptor.shaderKey = draw.translucent ? kTranslucentWorldShaderKey
                                            : kOpaqueWorldShaderKey;
    // Objects sharing a mesh merge into one instanced draw.
    descriptor.materialKey = mesh;

    assets_->acquire(mesh);
    ::pictor::ObjectId object = ::pictor::INVALID_OBJECT_ID;
    try {
        object = registry_->register_object(descriptor);
        tints_.emplace(object, draw.tint);
        bindings_.emplace(key, Binding{
                                   .object = object,
                                   .mesh = mesh,
                                   .translucent = draw.translucent,
                                   .model = draw.model,
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

void PresentationObjectSync::unregisterObject(
    const BindingMap::iterator binding, const std::uint64_t frameSerial) {
    const Binding removed = binding->second;
    bindings_.erase(binding);
    tints_.erase(removed.object);
    registry_->unregister_object(removed.object);
    assets_->release(removed.mesh, frameSerial);
}

void PresentationObjectSync::clear(const std::uint64_t frameSerial) noexcept {
    while (!bindings_.empty()) {
        const Binding removed = bindings_.begin()->second;
        bindings_.erase(bindings_.begin());
        registry_->unregister_object(removed.object);
        if (assets_->resolve(removed.mesh) != nullptr) {
            try {
                assets_->release(removed.mesh, frameSerial);
            } catch (...) {
                // Bookkeeping is already inconsistent; the asset store's own
                // shutdown still frees the buffers.
            }
        }
    }
    tints_.clear();
}

std::size_t PresentationObjectSync::objectCount() const noexcept {
    return bindings_.size();
}

std::optional<::pictor::ObjectId> PresentationObjectSync::objectFor(
    const render::PresentationObjectKey& key) const noexcept {
    const auto binding = bindings_.find(key);
    if (binding == bindings_.end()) {
        return std::nullopt;
    }
    return binding->second.object;
}

const render::WorldColor* PresentationObjectSync::tintFor(
    const ::pictor::ObjectId object) const noexcept {
    const auto tint = tints_.find(object);
    return tint == tints_.end() ? nullptr : &tint->second;
}

}  // namespace konbini::adapters::pictor
