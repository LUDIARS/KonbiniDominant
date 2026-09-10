// @implements spec/feature/grid-town-and-vector-ui.md Grid town
#include "konbini/render/grid_ground.h"
#include <cmath>
#include <stdexcept>
#include "konbini/city/grid_town.h"

namespace konbini::render {
namespace {
void rectangle(WorldMesh& mesh, double x0, double z0, double x1, double z1,
               float y, WorldVertex::ColorRgba color) {
    const auto first = static_cast<std::uint32_t>(mesh.vertices.size());
    for (const auto point : {sim::Vec3{x0,y,z0}, sim::Vec3{x1,y,z0},
                             sim::Vec3{x1,y,z1}, sim::Vec3{x0,y,z1}})
        mesh.vertices.push_back({{static_cast<float>(point.x),y,static_cast<float>(point.z)}, {0,1,0}, color});
    mesh.indices.insert(mesh.indices.end(), {first,first+2,first+1,first,first+3,first+2});
}
}
WorldMesh buildGridGround(std::span<const sim::RenderFacility> cells) {
    WorldMesh mesh;
    const double radius = city::kGridCellMeters * 0.5;
    for (const auto& cell : cells) {
        if (!cell.isBuildable) continue;
        const auto p = cell.positionMeters;
        const int x = static_cast<int>(std::floor(p.x / city::kGridCellMeters));
        const int z = static_cast<int>(std::floor(p.z / city::kGridCellMeters));
        const auto color = (x+z)%2 == 0 ? WorldVertex::ColorRgba{0.64F,0.58F,0.40F,1}
                                       : WorldVertex::ColorRgba{0.60F,0.54F,0.36F,1};
        rectangle(mesh,p.x-radius,p.z-radius,p.x+radius,p.z+radius,-0.02F,color);
        constexpr float lineY = 0.0F;
        constexpr double stroke = 0.10;
        const WorldVertex::ColorRgba line{0.29F,0.31F,0.19F,1};
        rectangle(mesh,p.x-radius,p.z-radius,p.x-radius+stroke,p.z+radius,lineY,line);
        rectangle(mesh,p.x+radius-stroke,p.z-radius,p.x+radius,p.z+radius,lineY,line);
        rectangle(mesh,p.x-radius,p.z-radius,p.x+radius,p.z-radius+stroke,lineY,line);
        rectangle(mesh,p.x-radius,p.z+radius-stroke,p.x+radius,p.z+radius,lineY,line);
    }
    return mesh;
}
std::optional<FacilityPick> pickGridCell(const WorldRay& ray,
        std::span<const sim::RenderFacility> cells, const double floorY) {
    if (!sim::isFinite(ray.originMeters) || !sim::isFinite(ray.direction) || !std::isfinite(floorY))
        throw std::invalid_argument("grid picking requires finite ray and floor");
    if (std::abs(ray.direction.y) < 1e-12) return std::nullopt;
    const double t = (floorY-ray.originMeters.y)/ray.direction.y;
    if (t < 0 || !std::isfinite(t)) return std::nullopt;
    const double x = ray.originMeters.x + t*ray.direction.x;
    const double z = ray.originMeters.z + t*ray.direction.z;
    for (const auto& cell : cells) {
        if (!cell.isBuildable) continue;
        const auto& b = cell.boundsMeters;
        if (x >= b.min.x && x < b.max.x && z >= b.min.z && z < b.max.z)
            return FacilityPick{cell.figmentumKey,cell.id,t};
    }
    return std::nullopt;
}
}
