#include "storefront_geometry.h"

#include <stdexcept>
#include <string_view>

#include "konbini/render/bitmap_font.h"
#include "konbini/render/world_palette.h"
#include "world_box_geometry.h"

// @implements spec/feature/three-store-brands.md Storefront geometry
namespace konbini::render::detail {
namespace {

// Coordinates are normalized to the requested width and roof height, keeping
// placement animation and the existing city footprint independent of branding.
class StorefrontBuilder {
public:
    StorefrontBuilder(WorldMesh& mesh, const sim::RenderStore& store,
                      const StoreMarkerSpec& spec)
        : mesh_(mesh), origin_(store.positionMeters), spec_(spec) {}

    void box(double x, double y, double z, double w, double h, double d,
             WorldColor color) {
        color[3] = spec_.alpha;
        appendAxisAlignedBox(mesh_,
            {origin_.x + x * spec_.halfWidthMeters,
             origin_.y + y * spec_.heightMeters,
             origin_.z + z * spec_.halfWidthMeters},
            {w * spec_.halfWidthMeters * 0.5,
             h * spec_.heightMeters * 0.5,
             d * spec_.halfWidthMeters * 0.5}, color);
    }

    void sign(std::string_view name, WorldColor ink) {
        if (name.empty()) {
            throw std::invalid_argument("storefront sign requires a name");
        }
        // 5 px glyphs with a 1 px gap; the trailing gap is not part of the band.
        const double pixel = 1.58 / static_cast<double>(name.size() * 6U - 1U);
        const double left = -0.79;
        for (std::size_t letter = 0; letter < name.size(); ++letter) {
            const auto& glyph = bitmapGlyph5x7(name[letter]);
            for (std::size_t row = 0; row < 7; ++row) {
                for (std::size_t col = 0; col < 5; ++col) {
                    if ((glyph[row] & (1U << (4U - col))) == 0) continue;
                    box(left + (static_cast<double>(letter * 6U + col) + 0.5) * pixel,
                        0.83 + (3.0 - static_cast<double>(row)) * 0.018,
                        0.814, pixel * 0.88, 0.016, 0.014, ink);
                }
            }
        }
    }

private:
    WorldMesh& mesh_;
    sim::Vec3 origin_;
    const StoreMarkerSpec& spec_;
};

}  // namespace

void appendStorefront(WorldMesh& mesh, const sim::RenderStore& store,
                      const StoreMarkerSpec& spec) {
    StorefrontBuilder b(mesh, store, spec);
    const WorldColor wall{0.91F, 0.87F, 0.77F, 1.0F};
    const WorldColor frame{0.19F, 0.18F, 0.23F, 1.0F};
    const WorldColor glass{0.22F, 0.42F, 0.47F, 1.0F};
    const WorldColor light{1.0F, 0.94F, 0.73F, 1.0F};
    const WorldColor primary = chainColor(store.chain, 1.0F);
    const bool moon = store.chain == sim::ChainId::Losan;
    const bool sun = store.chain == sim::ChainId::Famoma;
    const WorldColor accent = sun ? WorldColor{0.97F, 0.76F, 0.25F, 1.0F}
                                  : WorldColor{0.45F, 0.27F, 0.55F, 1.0F};

    b.box(0, 0.025, 0, 2, 0.05, 1.9, frame); // raised foundation
    b.box(0, 0.415, -0.06, 1.86, 0.78, 1.5, wall);
    b.box(0, 0.91, -0.03, 1.96, 0.10, 1.62, primary);
    b.box(0, 0.83, 0.755, 1.96, 0.18, 0.10, moon ? primary : wall);
    b.box(0, 0.722, 0.79, 1.96, 0.026, 0.11, accent);
    b.box(0, 0.963, -0.05, 1.72, 0.016, 1.32, frame); // inset flat roof
    b.box(0.51, 0.987, -0.33, 0.30, 0.024, 0.34, wall); // roof vent

    const double doorX = moon ? -0.35 : sun ? 0.38 : 0.0;
    // Continuous glazing, with opaque stylized reflection strips for readable
    // facades in the current opaque world pipeline (no transparency sorting).
    b.box(0, 0.40, 0.705, 1.70, 0.55, 0.045, glass);
    for (double x : {-0.85, -0.56, 0.0, 0.56, 0.85}) {
        b.box(x, 0.40, 0.743, 0.025, 0.55, 0.025, frame);
    }
    b.box(0, 0.20, 0.752, 1.68, 0.024, 0.014, primary);
    b.box(-0.55, 0.55, 0.753, 0.43, 0.024, 0.015, light);
    b.box(0.56, 0.55, 0.753, 0.40, 0.024, 0.015, light);
    b.box(doorX, 0.335, 0.772, 0.43, 0.61, 0.044, frame);
    b.box(doorX, 0.34, 0.800, 0.37, 0.54, 0.018, glass);
    b.box(doorX, 0.34, 0.816, 0.018, 0.54, 0.014, wall);
    for (double side : {-1.0, 1.0}) {
        b.box(doorX + side * 0.039, 0.32, 0.829, 0.012, 0.09, 0.018, light);
    }
    b.box(doorX, 0.056, 0.84, 0.55, 0.012, 0.19, primary);
    // Side-window and a bollard pair preserve the convenience-store silhouette
    // when seen from the game's elevated camera.
    b.box(0.942, 0.43, -0.05, 0.018, 0.39, 0.93, glass);
    for (double z : {-0.50, -0.05, 0.40}) {
        b.box(0.956, 0.43, z, 0.018, 0.41, 0.022, frame);
    }
    for (double x : {-0.80, 0.80}) {
        b.box(x, 0.135, 0.87, 0.045, 0.20, 0.045, accent);
    }

    if (moon) {
        // Deep porch and paired columns evoke a quiet neighborhood shop.
        b.box(-0.35, 0.695, 0.82, 0.94, 0.065, 0.23, primary);
        for (double x : {-0.77, 0.07}) {
            b.box(x, 0.36, 0.90, 0.047, 0.60, 0.047, wall);
        }
        b.sign("MOONPANTRY", light);
    } else if (sun) {
        // Segmented cheerful awning and a vertical end-cap.
        for (int i = 0; i < 9; ++i) {
            b.box(-0.84 + i * 0.21, 0.69, 0.82, 0.21, 0.06, 0.22,
                  i % 2 == 0 ? primary : accent);
        }
        b.box(-0.91, 0.44, 0.76, 0.10, 0.58, 0.08, primary);
        b.sign("SUNFOLD", frame);
    } else {
        // Two continuous bands wrap the front and sides, without reproducing
        // the reference chain's tricolor or numeral mark.
        b.box(0, 0.91, -0.03, 1.98, 0.022, 1.64, accent);
        b.box(0, 0.69, 0.81, 1.96, 0.035, 0.22, primary);
        b.sign("DAYLARK", frame);
    }
}

}  // namespace konbini::render::detail
