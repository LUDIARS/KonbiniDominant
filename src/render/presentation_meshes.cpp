#include "konbini/render/presentation_meshes.h"

#include <cmath>
#include <stdexcept>

#include "konbini/render/speech_glyph_atlas.h"
#include "konbini/render/visia.h"
#include "konbini/render/visia_geometry.h"

// @implements spec/interface/pictor-rendering.md Presentation objects

namespace konbini::render {

namespace {

constexpr WorldVertex::ColorRgba kWhite{1.0F, 1.0F, 1.0F, 1.0F};
constexpr WorldVertex::Normal kUnshaded{0.0F, 0.0F, 0.0F};

[[nodiscard]] WorldVertex flatVertex(const float x, const float y) noexcept {
    return {.position = {x, y, 0.0F}, .normal = kUnshaded, .color = kWhite};
}

[[nodiscard]] WorldMesh bubbleBackground() {
    WorldMesh mesh;
    mesh.vertices = {flatVertex(-0.5F, 0.0F), flatVertex(0.5F, 0.0F),
                     flatVertex(0.5F, 1.0F), flatVertex(-0.5F, 1.0F)};
    mesh.indices = {0, 1, 2, 0, 2, 3};
    return mesh;
}

[[nodiscard]] WorldMesh bubbleTail() {
    WorldMesh mesh;
    mesh.vertices = {flatVertex(-1.0F, 0.0F), flatVertex(0.0F, -1.0F),
                     flatVertex(1.0F, 0.0F)};
    mesh.indices = {0, 1, 2};
    return mesh;
}

}  // namespace

std::uint32_t landingRingFrame(const double normalizedAge) {
    if (!std::isfinite(normalizedAge) || normalizedAge < 0.0 ||
        normalizedAge > 1.0) {
        throw std::invalid_argument("landing ring age must be in [0, 1]");
    }
    return static_cast<std::uint32_t>(std::lround(
        normalizedAge * static_cast<double>(kLandingRingFrameCount - 1)));
}

// @implements spec/interface/pictor-rendering.md Presentation objects
std::vector<PresentationMesh> buildPresentationMeshes() {
    std::vector<PresentationMesh> meshes;
    meshes.push_back({
        .key = {PresentationMeshKind::ResidentBody, 0},
        .mesh = buildResidentPrimitiveGeometry(residentPrimitiveVisia(), {}),
    });
    for (std::uint32_t frame = 0; frame < kLandingRingFrameCount; ++frame) {
        const double age = static_cast<double>(frame) /
                           static_cast<double>(kLandingRingFrameCount - 1);
        meshes.push_back({
            .key = {PresentationMeshKind::LandingRingFrame, frame},
            .mesh = buildStoreLandingEffectPrimitiveGeometry(
                storeLandingEffectPrimitiveVisia(), {}, age),
        });
    }
    meshes.push_back({.key = {PresentationMeshKind::BubbleBackground, 0},
                      .mesh = bubbleBackground()});
    meshes.push_back({.key = {PresentationMeshKind::BubbleTail, 0},
                      .mesh = bubbleTail()});
    for (const SpeechGlyph& glyph : speechGlyphs()) {
        if (glyph.indices.empty()) {
            continue;
        }
        meshes.push_back({
            .key = {PresentationMeshKind::SpeechGlyph,
                    static_cast<std::uint32_t>(glyph.codepoint)},
            .mesh = buildSpeechGlyphMesh(glyph, kWhite),
        });
    }
    return meshes;
}

}  // namespace konbini::render
