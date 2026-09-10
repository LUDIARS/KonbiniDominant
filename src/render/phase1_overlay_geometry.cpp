#include "konbini/render/phase1_overlay_geometry.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include "konbini/render/world_palette.h"
#include "konbini/render/facility_display_bounds.h"
namespace konbini::render {
namespace {
float coordinate(const double value) {
    if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max()) {
        throw std::invalid_argument("phase1 overlay has an invalid coordinate");
    }
    return static_cast<float>(value);
}
std::uint32_t vertex(WorldMesh& mesh, const sim::Vec3 p, const float height, const WorldColor color) {
    if (mesh.vertices.size() >= std::numeric_limits<std::uint32_t>::max()) {
        throw std::overflow_error("phase1 overlay exceeds index range");
    }
    const auto index = static_cast<std::uint32_t>(mesh.vertices.size());
    // Store-derived overlays remain visible in high-floor bands.
    // @implements spec/feature/full-campaign-baseline.md Vertical presentation
    mesh.vertices.push_back(
        {{coordinate(p.x), coordinate(p.y + height), coordinate(p.z)}, {0, 1, 0}, color});
    return index;
}
void quad(WorldMesh& mesh, const std::array<sim::Vec3, 4>& points,
          const float height, const WorldColor color) {
    const auto a = vertex(mesh, points[0], height, color);
    const auto b = vertex(mesh, points[1], height, color);
    const auto c = vertex(mesh, points[2], height, color);
    const auto d = vertex(mesh, points[3], height, color);
    mesh.indices.insert(mesh.indices.end(), {a, c, b, a, d, c});
}
void strip(WorldMesh& mesh, const sim::Vec3 a, const sim::Vec3 b,
           const double halfWidth, const float height, const WorldColor color) {
    const double dx = b.x-a.x, dz = b.z-a.z, length = std::hypot(dx, dz);
    if (length == 0.0) { return; }
    const double x = -dz / length * halfWidth, z = dx / length * halfWidth;
    quad(mesh, {{{a.x+x, a.y, a.z+z}, {a.x-x, a.y, a.z-z},
                 {b.x-x, b.y, b.z-z}, {b.x+x, b.y, b.z+z}}}, height, color);
}
void square(WorldMesh& mesh, const sim::Vec3 center, const double half,
            const float height, const WorldColor color) {
    quad(mesh, {{{center.x-half, center.y, center.z-half}, {center.x+half, center.y, center.z-half},
                 {center.x+half, center.y, center.z+half}, {center.x-half, center.y, center.z+half}}},
         height, color);
}
}
// @implements spec/feature/phase-1-game-loop.md Match contract
// @implements spec/feature/full-campaign-baseline.md Vertical presentation
WorldMesh buildPhase1OverlayGeometry(const sim::RenderSnapshot& snapshot) {
    WorldMesh mesh;
    for (const auto& triangle : snapshot.triangles()) {
        const auto& p = triangle.points;
        const auto a = vertex(mesh, p[0], 0.08F, chainColor(triangle.chain, 0.22F));
        const auto b = vertex(mesh, p[1], 0.08F, chainColor(triangle.chain, 0.22F));
        const auto c = vertex(mesh, p[2], 0.08F, chainColor(triangle.chain, 0.22F));
        const double winding = (p[1].x-p[0].x)*(p[2].z-p[0].z) -
                               (p[1].z-p[0].z)*(p[2].x-p[0].x);
        if (winding > 0.0) { mesh.indices.insert(mesh.indices.end(), {a, c, b}); }
        else { mesh.indices.insert(mesh.indices.end(), {a, b, c}); }
        for (std::size_t i = 0; i < 3; ++i) {
            strip(mesh, p[i], p[(i+1)%3], 0.45, 0.12F, chainColor(triangle.chain, 0.95F));
        }
    }
    for (const auto& facility : snapshot.facilities()) {
        if (facility.state == sim::FacilityState::Destroyed) {
            square(mesh, facility.positionMeters, kDestroyedLotHalfWidthMeters,
                   kDestroyedLotHeightMeters, {0.45F, 0.31F, 0.24F, 0.8F});
        }
    }
    for (const auto& cell : snapshot.populationCells()) {
        const auto color = cell.owner ? chainColor(*cell.owner, 0.8F) :
                                       WorldColor{0.6F, 0.65F, 0.67F, 0.55F};
        square(mesh, cell.positionMeters, cell.population == 0 ? 0.35 : 1.0, 0.18F, color);
    }
    constexpr unsigned kWarningSegments = 32;
    constexpr double kTwoPi = 6.28318530717958647692;
    for (const auto& store : snapshot.stores()) {
        if (!store.isEncircled) { continue; }
        const auto total = store.captureDelayTotal ? store.captureDelayTotal : snapshot.hud().captureDelayTicks;
        if (total == 0) { throw std::logic_error("encirclement warning has no duration"); }
        const double fraction = std::clamp(static_cast<double>(store.captureTicksRemaining) / total, 0.0, 1.0);
        for (unsigned i = 0; i < kWarningSegments; ++i) {
            if (static_cast<double>(i) / kWarningSegments >= fraction) { break; }
            const double a = kTwoPi * i / kWarningSegments;
            const double b = kTwoPi * (i+1) / kWarningSegments;
            const sim::Vec3 p{store.positionMeters.x+5.0*std::cos(a), store.positionMeters.y,
                              store.positionMeters.z+5.0*std::sin(a)};
            const sim::Vec3 q{store.positionMeters.x+5.0*std::cos(b), store.positionMeters.y,
                              store.positionMeters.z+5.0*std::sin(b)};
            strip(mesh, p, q, 0.65, 0.3F, {1.0F, 0.2F, 0.05F, 1.0F});
        }
    }
    return mesh;
}
}  // namespace konbini::render
