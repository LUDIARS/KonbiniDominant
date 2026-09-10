#pragma once
#include <span>
#include "ergo/particle/particle_system.h"
#include "konbini/render/isometric_camera.h"
#include "konbini/render/store_construction_visual.h"
#include "konbini/render/world_mesh.h"
namespace konbini::adapters::ergo::detail {
void appendConstructionParticles(render::WorldMesh& mesh,
    std::span<const ::ergo::particle::ParticleInstance> particles,
    const render::StoreConstructionVisual& visual,
    const render::IsometricCamera& camera, bool dust);
}
