# Store construction effects

## Choreography

User direction (2026-09-10): turn once, float briefly, slam onto the ground;
show a turning effect and landing dust using Ergo's effect system.

- 0.36 s: rotate 360 degrees while easing upward by 8 m.
- 0.16 s: hover with a small 0.55 m bob.
- 0.18 s: cubic acceleration down to the exact target position and yaw.
- 0.60 s: a burst of expanding, fading earth-colored dust.
- Two warm particle arcs follow the spin. No camera shake or full-screen flash.
- The accepted placement takes effect immediately for gameplay; these timings
  use presentation seconds and remain readable during accelerated play.

## Connected stores

An airborne grid cell renders as a standalone square shop, including its sign.
At impact it joins settled neighbors, so the facade and horizontal sign expand.
Only cells from the same dimension and visible floor band are drawn.

## Ergo particles

Use the pinned Ergo ergo_particle CPU module: ParticleEffectConfig,
ParticleSystem::set_config/update/burst/instances. Spin and dust presets live
in the KD Ergo adapter. Apply configuration with update(0) before burst().
Ergo manages emission, velocities, damping, lifetime, size and color; KD maps
the module's XY particle data into world space and soft billboard triangles.
The existing Pictor world overlay handles Vulkan alpha blending and depth.
No new shaders, external service, textures, or changes to the Ergo checkout.

## Playback lifecycle

Consume every completed simulation snapshot, including intermediate catch-up
ticks, once. Retain successful placement cues across display frames; cancel
effects for destroyed stores, world switches, retry, and results. Explicit
pause stops presentation time. Up to 64 simultaneous constructions have effects;
overflow remains fully visible at its final pose. Gameplay state and canonical
snapshots do not contain effects or renderer randomness.

## Verification

Check phase boundaries, exact final pose on elevated floors, bounded lifetimes,
repeated and intermediate snapshots, destruction/retry cleanup, actual Ergo
particle geometry, and connection geometry after impact. Run the native game
from the project body with Cc claim/release, verify a normal-speed placement
visually, and perform the existing no-LLM 100x campaign-clear acceptance run.
