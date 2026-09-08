#include "konbini/render/store_tower_geometry.h"

#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "konbini/render/bitmap_font.h"
#include "world_box_geometry.h"
#include "storefront_geometry.h"
#include "store_tower_additions.h"

// @implements spec/feature/thirty-floor-store-tower.md Geometry
namespace konbini::render {
namespace {

void appendFloorNumber(WorldMesh& mesh, const sim::Vec3 center,
                       const std::uint32_t floor, const double scale) {
    // Two digit cells only. A three-digit floor would silently render a
    // non-digit glyph for the tens place, so reject it instead.
    if (floor > 99U) {
        throw std::invalid_argument("floor number plate holds two digits");
    }
    const WorldVertex::ColorRgba panel{0.12F, 0.14F, 0.20F, 1.0F};
    const WorldVertex::ColorRgba ink{1.0F, 0.90F, 0.56F, 1.0F};
    detail::appendAxisAlignedBox(mesh, center, {0.80 * scale, 0.57 * scale, 0.08 * scale}, panel);
    const std::array digits{static_cast<char>('0' + floor / 10U),
                            static_cast<char>('0' + floor % 10U)};
    constexpr double pixel = 0.105;
    for (std::size_t digit = 0; digit < digits.size(); ++digit) {
        const auto& glyph = bitmapGlyph5x7(digits[digit]);
        for (std::size_t row = 0; row < 7; ++row) {
            for (std::size_t col = 0; col < 5; ++col) {
                if ((glyph[row] & (1U << (4U - col))) == 0) continue;
                detail::appendAxisAlignedBox(mesh,
                    {center.x + (static_cast<double>(digit * 6U + col) - 5.0) * pixel * scale,
                     center.y + (3.0 - static_cast<double>(row)) * pixel * scale,
                     center.z + 0.09 * scale},
                    {pixel * scale * 0.44, pixel * scale * 0.44, 0.015 * scale}, ink);
            }
        }
    }
}

}  // namespace

WorldMesh buildStoreTowerGeometry(const sim::Vec3 origin, const StoreTowerSpec& spec) {
    if (!sim::isFinite(origin) || !std::isfinite(spec.floor.heightMeters) ||
        spec.floor.heightMeters <= 0.0 || !std::isfinite(spec.floor.halfWidthMeters) ||
        spec.floor.halfWidthMeters <= 0.0 || spec.floor.alpha != 1.0F) {
        throw std::invalid_argument("tower requires finite dimensions and opaque floors");
    }
    constexpr std::array brands{sim::ChainId::SebanIleban, sim::ChainId::Losan, sim::ChainId::Famoma};
    std::vector<sim::RenderStore> floors;
    floors.reserve(StoreTowerSpec::floorCount);
    for (std::uint32_t floor = 0; floor < StoreTowerSpec::floorCount; ++floor) {
        floors.push_back({.id = {{floor, 1}}, .facilityId = {{floor, 1}},
            .chain = brands[floor % brands.size()],
            .positionMeters = {origin.x, origin.y + floor * spec.floor.heightMeters, origin.z}});
    }
    WorldMesh mesh;
    for (std::size_t i = 0; i < floors.size(); ++i) {
        auto store = floors[i];
        auto floorSpec = spec.floor;
        // Repeatable offsets suggest incremental rebuilding, while the storey
        // heights and exactly thirty branded storefronts remain unchanged.
        store.positionMeters.x += (static_cast<int>(i % 5) - 2) * 0.28 * spec.floor.halfWidthMeters / 6.0;
        store.positionMeters.z += (static_cast<int>(i % 3) - 1) * 0.38 * spec.floor.halfWidthMeters / 6.0;
        floorSpec.halfWidthMeters *= 1.0 + static_cast<double>(i % 4) * 0.025;
        detail::appendStorefront(mesh, store, floorSpec);
    }
    detail::appendStoreTowerAdditions(mesh, origin, spec);
    const double scale = spec.floor.halfWidthMeters / 6.0;
    const double height = StoreTowerSpec::floorCount * spec.floor.heightMeters;
    const double spineX = origin.x + spec.floor.halfWidthMeters + 0.4 * scale;
    const double spineZ = origin.z + spec.floor.halfWidthMeters * 1.28;
    // A narrow continuous service spine supports the externally readable floor
    // plates. It adds no extra storefront floor or simulation entity.
    detail::appendAxisAlignedBox(mesh, {spineX, origin.y + height * 0.5, spineZ - 0.2 * scale},
        {0.23 * scale, height * 0.5, 0.23 * scale}, {0.35F, 0.37F, 0.43F, 1.0F});
    for (std::uint32_t floor = 0; floor < StoreTowerSpec::floorCount; ++floor) {
        appendFloorNumber(mesh,
            {spineX, origin.y + (floor + 0.5) * spec.floor.heightMeters, spineZ}, floor + 1U, scale);
    }
    return mesh;
}

}  // namespace konbini::render
