// @implements spec/feature/store-construction-effects.md Ergo particles
#include "construction_particle_presets.h"
namespace konbini::adapters::ergo::detail {
::ergo::particle::ParticleEffectConfig constructionSpinPreset() {
    ::ergo::particle::ParticleEffectConfig c;
    c.name = "konbini-construction-spin";
    c.emission_rate = 70; c.emission_max_alive = 32;
    c.init_position_radius = 0.18F;
    c.init_velocity_angle_deg = 180; c.init_velocity_spread_deg = 12;
    c.init_speed_min = 7; c.init_speed_max = 12;
    c.init_lifetime_min = 0.20F; c.init_lifetime_max = 0.32F;
    c.init_size = 0.52F;
    c.life_size_start = 1; c.life_size_end = 0.08F;
    c.life_color_start = {1.0F, 0.88F, 0.42F, 0.95F};
    c.life_color_end = {0.45F, 0.8F, 1.0F, 0};
    c.life_velocity_damping = 0.85F; c.gravity = {0, 0};
    c.render_blend = ::ergo::particle::BlendMode::Alpha;
    return c;
}
::ergo::particle::ParticleEffectConfig constructionDustPreset() {
    auto c = constructionSpinPreset();
    c.name = "konbini-construction-dust";
    c.emission_rate = 0; c.emission_max_alive = 56;
    c.init_position_radius = 7.8F;
    c.init_velocity_angle_deg = 0; c.init_velocity_spread_deg = 360;
    c.init_speed_min = 3; c.init_speed_max = 9;
    c.init_lifetime_min = 0.40F; c.init_lifetime_max = 0.60F;
    c.init_size = 1.3F;
    c.life_size_start = 0.65F; c.life_size_end = 2.6F;
    c.life_color_start = {0.78F, 0.66F, 0.44F, 0.64F};
    c.life_color_end = {0.62F, 0.54F, 0.40F, 0};
    c.life_velocity_damping = 0.12F;
    return c;
}
}
