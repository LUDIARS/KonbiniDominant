#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <unordered_map>
#include <utility>

#include "konbini/adapters/pictor/object_tint_source.h"
#include "konbini/render/presentation_draw.h"

#ifndef NOGDI
#define NOGDI
#endif
#include "pictor/scene/scene_registry.h"

// @implements spec/interface/pictor-rendering.md Presentation objects

namespace konbini::adapters::pictor {

class GpuAssetStore;

struct PresentationSyncReport {
    std::uint32_t added = 0;
    std::uint32_t removed = 0;
    // Same key, different mesh or pass: unregistered and re-registered.
    std::uint32_t rebound = 0;
    // Same key and mesh, new transform (bounds follow the transform).
    std::uint32_t moved = 0;
    std::uint32_t retinted = 0;
};

// Keeps `PresentationObjectKey → Pictor ObjectId` in step with the frame's
// presentation draws (pictor-rendering.md#Presentation objects).
//
// Same diff rules as `PictorSceneSync`, plus a per-object transform:
// - Added → resolve the shared mesh, acquire it, register with the model
//   matrix and the mesh's local bounds transformed to world space.
// - Removed (resident left the snapshot, effect expired, bubble culled) →
//   unregister and release the mesh reference in the same apply.
// - Changed mesh / pass → re-register.
// - Moved → `update_transform` + `update_bounds`; the ObjectId is kept, so a
//   resident keeps its Pictor object for as long as its stable id lives.
//
// Shared meshes are never uploaded here: they come from
// `loadPresentationGeometry` at startup, and an unknown mesh key is
// `std::out_of_range` before the registry changes.
//
// `registry` and `assets` are borrowed and must outlive this sync.
class PresentationObjectSync final : public IObjectTintSource {
public:
    PresentationObjectSync(::pictor::SceneRegistry& registry,
                           GpuAssetStore& assets);
    ~PresentationObjectSync() override;

    PresentationObjectSync(const PresentationObjectSync&) = delete;
    PresentationObjectSync& operator=(const PresentationObjectSync&) = delete;

    // Duplicate keys, non-finite models / tints are `std::invalid_argument`;
    // a mesh missing from the asset store is `std::out_of_range`. Validation
    // finishes before any registry change.
    PresentationSyncReport apply(
        std::span<const render::PresentationDraw> draws,
        std::uint64_t frameSerial);
    // The validation half of `apply` without touching the registry, so the
    // bridge can reject a frame before the facility sync changes anything.
    void validate(std::span<const render::PresentationDraw> draws) const;

    // Unregisters every object and releases its mesh reference.
    void clear(std::uint64_t frameSerial) noexcept;

    [[nodiscard]] std::size_t objectCount() const noexcept;
    [[nodiscard]] std::optional<::pictor::ObjectId> objectFor(
        const render::PresentationObjectKey& key) const noexcept;
    [[nodiscard]] const render::WorldColor* tintFor(
        ::pictor::ObjectId object) const noexcept override;

private:
    struct Binding {
        ::pictor::ObjectId object = ::pictor::INVALID_OBJECT_ID;
        ::pictor::MeshHandle mesh = ::pictor::INVALID_MESH;
        bool translucent = false;
        std::array<float, 16> model{};
    };
    using BindingMap = std::map<render::PresentationObjectKey, Binding>;
    using WantedMap =
        std::map<render::PresentationObjectKey,
                 std::pair<const render::PresentationDraw*, ::pictor::MeshHandle>>;

    [[nodiscard]] WantedMap collect(
        std::span<const render::PresentationDraw> draws) const;

    void registerObject(const render::PresentationObjectKey& key,
                        ::pictor::MeshHandle mesh,
                        const render::PresentationDraw& draw);
    void unregisterObject(BindingMap::iterator binding,
                          std::uint64_t frameSerial);
    [[nodiscard]] ::pictor::AABB worldBounds(
        ::pictor::MeshHandle mesh, const std::array<float, 16>& model) const;

    ::pictor::SceneRegistry* registry_ = nullptr;
    GpuAssetStore* assets_ = nullptr;
    BindingMap bindings_;
    std::unordered_map<::pictor::ObjectId, render::WorldColor> tints_;
};

}  // namespace konbini::adapters::pictor
