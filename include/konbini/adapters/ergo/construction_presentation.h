#pragma once
#include <memory>
#include <span>
#include "konbini/render/isometric_camera.h"
#include "konbini/render/store_construction_visual.h"
#include "konbini/render/world_mesh.h"
namespace konbini::adapters::ergo {
class ConstructionPresentation {
public:
    ConstructionPresentation();
    ~ConstructionPresentation();
    void observe(const sim::RenderSnapshot& snapshot);
    void advance(double deltaSeconds);
    void reset();
    [[nodiscard]] std::span<const render::StoreConstructionVisual> visuals() const;
    [[nodiscard]] render::WorldMesh particleMesh(
        const render::IsometricCamera& camera, std::span<const sim::RenderStore> visibleStores) const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
