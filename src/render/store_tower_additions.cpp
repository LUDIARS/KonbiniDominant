#include "store_tower_additions.h"

#include <array>
#include <stdexcept>
#include <string_view>

#include "konbini/render/bitmap_font.h"
#include "world_box_geometry.h"

// @implements spec/feature/thirty-floor-store-tower.md Dense additions
namespace konbini::render::detail {
namespace {
using Color = WorldVertex::ColorRgba;
constexpr Color concrete{0.48F, 0.47F, 0.39F, 1.0F};
constexpr Color rust{0.43F, 0.23F, 0.16F, 1.0F};
constexpr Color steel{0.16F, 0.21F, 0.23F, 1.0F};
constexpr Color warm{1.0F, 0.76F, 0.36F, 1.0F};
constexpr Color teal{0.16F, 0.68F, 0.65F, 1.0F};

class AnnexBuilder {
public:
    AnnexBuilder(WorldMesh& mesh, sim::Vec3 origin, const StoreTowerSpec& spec)
        : mesh_(mesh), origin_(origin), widthScale_(spec.floor.halfWidthMeters / 6.0),
          heightScale_(spec.floor.heightMeters / 3.6) {}

    void box(double x, double y, double z, double w, double h, double d, Color color) {
        appendAxisAlignedBox(mesh_,
            {origin_.x + x * widthScale_, origin_.y + y * heightScale_, origin_.z + z * widthScale_},
            {w * widthScale_ / 2, h * heightScale_ / 2, d * widthScale_ / 2}, color);
    }

    void verticalSign(double x, double y, double z, std::string_view text, Color ink) {
        if (text.empty()) {
            throw std::invalid_argument("vertical sign requires text");
        }
        const double height = static_cast<double>(text.size()) * 1.35 + 0.4;
        box(x, y, z, 1.55, height, 0.35, steel);
        box(x - 0.73, y, z + 0.2, 0.07, height, 0.08, ink);
        box(x + 0.73, y, z + 0.2, 0.07, height, 0.08, ink);
        for (std::size_t letter = 0; letter < text.size(); ++letter) {
            const auto& glyph = bitmapGlyph5x7(text[letter]);
            for (std::size_t row = 0; row < 7; ++row) {
                for (std::size_t col = 0; col < 5; ++col) {
                    if ((glyph[row] & (1U << (4U - col))) == 0) continue;
                    box(x + (static_cast<double>(col) - 2) * 0.20,
                        y + height / 2 - 0.45 -
                            static_cast<double>(letter) * 1.35 -
                            static_cast<double>(row) * 0.16,
                        z + 0.21, 0.17, 0.14, 0.09, ink);
                }
            }
        }
    }

    void annex(int floor, double side) {
        const double y = floor * 3.6;
        const double width = 4.8 + (floor % 4) * 0.6;
        const double x = side * (6.0 + width / 2);
        const double front = 4.2 + (floor % 3) * 0.65;
        constexpr std::array<Color, 4> plaster{concrete,
            Color{0.57F, 0.52F, 0.41F, 1}, Color{0.36F, 0.43F, 0.42F, 1},
            Color{0.56F, 0.42F, 0.34F, 1}};
        box(x, y + 1.72, front - 3.8, width, 3.44, 7.6, plaster[floor % 4]);
        box(x, y + 0.12, front, width + 0.5, 0.24, 1.3, steel);
        for (int window = 0; window < 3; ++window) {
            const double wx = x + (window - 1) * 1.40;
            box(wx, y + 1.95, front + 0.035, 1.05, 1.6, 0.12,
                (floor + window) % 4 == 0 ? warm : steel);
            box(wx, y + 1.95, front + 0.12, 0.06, 1.6, 0.08, concrete);
            box(wx, y + 2.0, front + 0.12, 1.05, 0.07, 0.08, concrete);
        }
        // Box air conditioners, individual louvers and patched tin awnings.
        box(x + side * 1.8, y + 0.9, front + 0.45, 1.1, 0.7, 0.7, concrete);
        for (int slat = 0; slat < 4; ++slat)
            box(x + side * 1.8, y + 0.68 + slat * 0.14, front + 0.82, 0.85, 0.045, 0.04, steel);
        if (floor % 3 == 0) {
            box(x, y + 3.15, front + 0.65, width + 0.4, 0.12, 1.9, rust);
            for (int rib = 0; rib < 9; ++rib)
                box(x - width / 2 + rib * width / 8, y + 3.23, front + 0.65, 0.05, 0.04, 1.9, concrete);
        }
        // External switchback stairs climb one storey per flight.
        const double stairX = x + side * (width / 2 + 0.7);
        for (int step = 0; step < 12; ++step) {
            const double z = -2.6 + (floor % 2 == 0 ? step : 11 - step) * 0.48;
            box(stairX, y + (step + 1) * 0.3, z, 1.35, 0.14, 0.52, rust);
            box(stairX + side * 0.62, y + (step + 1) * 0.3 + 0.5, z, 0.07, 1.0, 0.07, steel);
        }
        box(stairX, y + 0.1, floor % 2 == 0 ? -2.8 : 3.0, 1.6, 0.2, 1.5, steel);
    }

private:
    WorldMesh& mesh_;
    sim::Vec3 origin_;
    double widthScale_;
    double heightScale_;
};
}  // namespace

void appendStoreTowerAdditions(WorldMesh& mesh, sim::Vec3 origin, const StoreTowerSpec& spec) {
    AnnexBuilder b(mesh, origin, spec);
    // Unequal rooflines keep the annexes legible as accumulated additions.
    for (int floor = 0; floor < 27; ++floor) b.annex(floor, -1.0);
    for (int floor = 0; floor < 24; ++floor) b.annex(floor, 1.0);
    for (int pipe = 0; pipe < 5; ++pipe) {
        const double x = -5.4 + pipe * 2.8;
        const double height = 85.0 + (pipe % 3) * 9.0;
        b.box(x, height / 2, -6.25, 0.16, height, 0.16, rust);
        b.box(x, height - 0.5, -4.4, 0.16, 0.16, 3.8, rust);
    }
    // Front drain stacks, cable trays and balcony rails visibly layer the facade.
    for (int floor = 0; floor < 30; ++floor) {
        const double y = floor * 3.6;
        b.box(-6.25, y + 1.8, 6.6, 0.14, 3.6, 0.14, rust);
        b.box(6.6, y + 1.8, 6.2, 0.12, 3.6, 0.12, teal);
        if (floor % 4 == 1) {
            b.box(0, y + 0.05, 6.2, 13.5, 0.16, 1.5, steel);
            b.box(0, y + 0.95, 6.85, 13.5, 0.08, 0.08, rust);
            for (int post = 0; post < 12; ++post)
                b.box(-6.3 + post * 1.14, y + 0.5, 6.85, 0.07, 0.9, 0.07, steel);
        }
    }
    b.verticalSign(-8.5, 89, 7.0, "OPEN", teal);
    b.verticalSign(9.0, 70, 7.2, "24H", warm);
    b.verticalSign(-7.7, 48, 7.6, "MART", warm);
    b.verticalSign(7.8, 29, 7.1, "FOOD", teal);
    // Rooftop tanks and antenna clusters sit on the lower annex terraces.
    for (double side : {-1.0, 1.0}) {
        const double roof = side < 0 ? 97.2 : 86.4;
        b.box(side * 8.4, roof + 1.3, -0.5, 2.4, 2.6, 2.4, concrete);
        b.box(side * 8.4, roof + 2.64, -0.5, 2.65, 0.18, 2.65, steel);
        b.box(side * 10.0, roof + 3.2, -2.0, 0.12, 6.4, 0.12, rust);
        for (int bar = 0; bar < 4; ++bar)
            b.box(side * 10.0, roof + 4.3 + bar * 0.5, -2.0, 2.4 - bar * 0.3, 0.08, 0.08, steel);
    }
}
}  // namespace konbini::render::detail
