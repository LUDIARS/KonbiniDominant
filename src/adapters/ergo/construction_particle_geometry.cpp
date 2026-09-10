// @implements spec/feature/store-construction-effects.md Ergo particles
#include "construction_particle_geometry.h"
#include "konbini/render/world_palette.h"
#include <algorithm>
#include <cmath>
#include <numbers>
namespace konbini::adapters::ergo::detail {
namespace {
void puff(render::WorldMesh& mesh, sim::Vec3 center, float radius,
          render::WorldColor color, const render::IsometricCamera& camera) {
    constexpr std::uint32_t segments = 10;
    const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
    const auto vertex = [&](sim::Vec3 p, render::WorldColor tint) {
        mesh.vertices.push_back({{static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z)},
                                 {0, 1, 0}, tint});
    };
    vertex(center, color);
    // A soft radial alpha edge gives texture-free dust a round silhouette.
    auto edge = color; edge[3] = 0;
    for (std::uint32_t i=0; i<segments; ++i) {
        const double a = i * 2.0 * std::numbers::pi / segments;
        const double x = std::cos(a)*radius, y = std::sin(a)*radius;
        vertex({center.x+camera.right.x*x+camera.up.x*y,
                center.y+camera.right.y*x+camera.up.y*y,
                center.z+camera.right.z*x+camera.up.z*y}, edge);
    }
    for (std::uint32_t i=0; i<segments; ++i)
        mesh.indices.insert(mesh.indices.end(), {base, base+1+i, base+1+(i+1)%segments});
}
}
void appendConstructionParticles(render::WorldMesh& mesh,
    std::span<const ::ergo::particle::ParticleInstance> particles,
    const render::StoreConstructionVisual& visual,
    const render::IsometricCamera& camera, bool dust) {
    for (const auto& p : particles) {
        const render::WorldColor color{p.color[0], p.color[1], p.color[2], p.color[3]};
        if (p.size <= 0 || color[3] <= 0) continue;
        if (dust) {
            const auto& at = visual.store.positionMeters;
            puff(mesh, {at.x+p.pos[0], at.y+0.25+std::max(0.0F,p.size-0.8F)*0.4,
                        at.z+p.pos[1]}, p.size, color, camera);
        } else {
            // Ergo's 2D particle trail is wrapped around the store in world
            // space. Two opposite arcs make the full turn readable from above.
            const auto& at = visual.animation.storePose;
            const double angle = at.yawDegrees*std::numbers::pi/180.0 + p.pos[0]*0.24;
            for (const double offset : {0.0, std::numbers::pi}) {
                const double a = angle+offset;
                puff(mesh, {at.positionMeters.x+std::cos(a)*8.6,
                            at.positionMeters.y+1.8+p.pos[1],
                            at.positionMeters.z+std::sin(a)*8.6},
                     p.size, color, camera);
            }
        }
    }
}
}
