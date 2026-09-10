#include "konbini/render/hud_text_geometry.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

#include "konbini/render/vector_font.h"

// @implements spec/feature/ui-ux.md Common HUD

namespace konbini::render {
namespace {

void appendQuad(
    WorldMesh& mesh, const float minX, const float minY, const float maxX,
    const float maxY, const WorldVertex::ColorRgba& color) {
    const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
    // normal は 0 ベクトルのまま。HUD は screen space の平面で、world pass の
    // 陰影計算とは別 pipeline を使う。
    mesh.vertices.push_back({{minX, minY, 0.0F}, {}, color});
    mesh.vertices.push_back({{maxX, minY, 0.0F}, {}, color});
    mesh.vertices.push_back({{maxX, maxY, 0.0F}, {}, color});
    mesh.vertices.push_back({{minX, maxY, 0.0F}, {}, color});
    mesh.indices.push_back(base);
    mesh.indices.push_back(base + 1);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 3);
}

void validateStyle(const HudTextStyle& style) {
    if (!std::isfinite(style.glyphPixelScale) ||
        style.glyphPixelScale <= 0.0F ||
        !std::isfinite(style.glyphSpacingPixels) ||
        style.glyphSpacingPixels < 0.0F ||
        !std::isfinite(style.linePaddingPixels) ||
        style.linePaddingPixels < 0.0F ||
        !std::isfinite(style.originXPixels) ||
        !std::isfinite(style.originYPixels) ||
        !std::isfinite(style.panelPaddingPixels) ||
        style.panelPaddingPixels < 0.0F) {
        throw std::invalid_argument("hud text style has invalid metrics");
    }
}

}  // namespace

HudTextStyle defaultHudTextStyle() noexcept {
    return {};
}

float hudLineHeightPixels(const HudTextStyle& style) {
    validateStyle(style);
    return 7.0F * style.glyphPixelScale +
           style.linePaddingPixels;
}

float hudTextWidthPixels(
    const std::string_view line, const HudTextStyle& style) {
    validateStyle(style);
    if (line.empty()) {
        return 0.0F;
    }
    float width = 0;
    for (const char character : line)
        width += vectorGlyph(character).advance * 7.0F * style.glyphPixelScale + style.glyphSpacingPixels;
    return width - style.glyphSpacingPixels;
}

// @implements spec/feature/ui-ux.md Common HUD
WorldMesh buildHudTextMesh(
    const std::span<const std::string> lines, const HudTextStyle& style,
    const ViewportExtent extent) {
    validateStyle(style);
    if (extent.width == 0 || extent.height == 0) {
        throw std::invalid_argument(
            "hud text geometry requires a non-zero viewport");
    }
    for (const std::string& line : lines) {
        validateVectorFontText(line);
    }

    WorldMesh mesh;
    if (lines.empty()) {
        return mesh;
    }

    const float lineHeight = hudLineHeightPixels(style);
    float widestLine = 0.0F;
    for (const std::string& line : lines) {
        widestLine = std::max(widestLine, hudTextWidthPixels(line, style));
    }

    if (style.panelColor[3] > 0.0F) {
        const float panelMinX = style.originXPixels - style.panelPaddingPixels;
        const float panelMinY = style.originYPixels - style.panelPaddingPixels;
        const float panelMaxX =
            style.originXPixels + widestLine + style.panelPaddingPixels;
        const float panelMaxY = style.originYPixels +
                                lineHeight * static_cast<float>(lines.size()) -
                                style.linePaddingPixels +
                                style.panelPaddingPixels;
        appendQuad(
            mesh, panelMinX, panelMinY, panelMaxX, panelMaxY,
            style.panelColor);
    }

    const float height = 7.0F * style.glyphPixelScale;
    for (std::size_t row = 0; row < lines.size(); ++row) {
        float left = style.originXPixels;
        const float top = style.originYPixels + lineHeight * static_cast<float>(row);
        for (const char character : lines[row]) {
            const auto glyph = vectorGlyph(character);
            const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
            for (const auto point : glyph.points) {
                mesh.vertices.push_back({{
                    left + static_cast<float>(point[0]) / kVectorFontUnits * height,
                    top + static_cast<float>(point[1]) / kVectorFontUnits * height, 0},
                    {}, style.textColor});
            }
            for (const auto index : glyph.indices) mesh.indices.push_back(base + index);
            left += glyph.advance * height + style.glyphSpacingPixels;
        }
    }
    return mesh;
}

}  // namespace konbini::render
