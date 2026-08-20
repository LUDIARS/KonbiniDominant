#include "konbini/render/hud_text_geometry.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

#include "konbini/render/bitmap_font.h"

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
    return static_cast<float>(kBitmapGlyphHeight) * style.glyphPixelScale +
           style.linePaddingPixels;
}

float hudTextWidthPixels(
    const std::string_view line, const HudTextStyle& style) {
    validateStyle(style);
    if (line.empty()) {
        return 0.0F;
    }
    const float advance =
        static_cast<float>(kBitmapGlyphWidth) * style.glyphPixelScale +
        style.glyphSpacingPixels;
    return advance * static_cast<float>(line.size()) -
           style.glyphSpacingPixels;
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
        validateBitmapFontText(line);
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

    const float pixel = style.glyphPixelScale;
    const float advance =
        static_cast<float>(kBitmapGlyphWidth) * pixel +
        style.glyphSpacingPixels;
    for (std::size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const float lineTop =
            style.originYPixels + lineHeight * static_cast<float>(lineIndex);
        const std::string& line = lines[lineIndex];
        for (std::size_t column = 0; column < line.size(); ++column) {
            const BitmapGlyphRows& glyph = bitmapGlyph5x7(line[column]);
            const float glyphLeft =
                style.originXPixels + advance * static_cast<float>(column);
            for (std::uint32_t row = 0; row < kBitmapGlyphHeight; ++row) {
                const std::uint8_t bits = glyph[row];
                for (std::uint32_t bit = 0; bit < kBitmapGlyphWidth; ++bit) {
                    // bit 4 が左端。行を右から左へ読むと文字が鏡像になる。
                    const std::uint8_t mask = static_cast<std::uint8_t>(
                        1U << (kBitmapGlyphWidth - 1U - bit));
                    if ((bits & mask) == 0) {
                        continue;
                    }
                    const float minX =
                        glyphLeft + static_cast<float>(bit) * pixel;
                    const float minY =
                        lineTop + static_cast<float>(row) * pixel;
                    appendQuad(
                        mesh, minX, minY, minX + pixel, minY + pixel,
                        style.textColor);
                }
            }
        }
    }
    return mesh;
}

}  // namespace konbini::render
