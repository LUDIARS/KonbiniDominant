#include "konbini/render/speech_bubble_geometry.h"

#include <cmath>
#include <limits>
#include <stdexcept>

#include "konbini/render/bitmap_font.h"

#include "visia_geometry_support.h"

// @implements spec/interface/visia-presentation.md Speech bubble geometry
// @implements spec/feature/npc-conversations-and-placement-feedback.md Conversations

namespace konbini::render {

namespace {

constexpr double kBasisTolerance = 1.0e-6;
constexpr std::size_t kMaximumSupportedCharacters = 64;

// Vector, float-range, color and vertex helpers are shared with the other
// Visia geometry builders so every builder emits identical world vertices and
// rejects the same out-of-range values.
using detail::add;
using detail::appendVertex;
using detail::isValidColor;
using detail::scale;

[[nodiscard]] sim::Vec3 subtract(const sim::Vec3 left,
                                 const sim::Vec3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] double dot(const sim::Vec3 left,
                         const sim::Vec3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] sim::Vec3 cross(const sim::Vec3 left,
                              const sim::Vec3 right) noexcept {
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

[[nodiscard]] bool isUnit(const sim::Vec3 value) noexcept {
    return std::abs(dot(value, value) - 1.0) <= kBasisTolerance;
}

[[nodiscard]] bool isOrthogonal(const sim::Vec3 left,
                                const sim::Vec3 right) noexcept {
    return std::abs(dot(left, right)) <= kBasisTolerance;
}

// A bubble is exactly one glyph row wide plus horizontal padding, so its
// background width is fully determined by the character count and the style.
// Both validation and emission derive it here to stay in agreement.
[[nodiscard]] double glyphWidthMeters(
    const SpeechBubbleStyle& style) noexcept {
    return static_cast<double>(kBitmapGlyphWidth) * style.pixelSizeMeters;
}

[[nodiscard]] double backgroundWidthMeters(
    const std::size_t characterCount,
    const SpeechBubbleStyle& style) noexcept {
    return static_cast<double>(characterCount) * glyphWidthMeters(style) +
           static_cast<double>(characterCount - 1U) *
               style.glyphSpacingMeters +
           2.0 * style.horizontalPaddingMeters;
}

void validateCamera(const IsometricCamera& camera) {
    if (!sim::isFinite(camera.eyeMeters) ||
        !sim::isFinite(camera.forward) ||
        !sim::isFinite(camera.right) ||
        !sim::isFinite(camera.up) ||
        !isUnit(camera.forward) || !isUnit(camera.right) ||
        !isUnit(camera.up) ||
        !isOrthogonal(camera.forward, camera.right) ||
        !isOrthogonal(camera.forward, camera.up) ||
        !isOrthogonal(camera.right, camera.up) ||
        dot(cross(camera.right, camera.forward), camera.up) <
            1.0 - kBasisTolerance ||
        camera.extent.width == 0 || camera.extent.height == 0) {
        throw std::invalid_argument(
            "speech bubbles require a valid isometric camera basis");
    }
}

void validateStyle(const SpeechBubbleStyle& style) {
    if (!std::isfinite(style.pixelSizeMeters) ||
        style.pixelSizeMeters <= 0.0 ||
        !std::isfinite(style.glyphSpacingMeters) ||
        style.glyphSpacingMeters < 0.0 ||
        !std::isfinite(style.horizontalPaddingMeters) ||
        style.horizontalPaddingMeters <= 0.0 ||
        !std::isfinite(style.verticalPaddingMeters) ||
        style.verticalPaddingMeters <= 0.0 ||
        !std::isfinite(style.tailHeightMeters) ||
        style.tailHeightMeters <= 0.0 ||
        !std::isfinite(style.tailHalfWidthMeters) ||
        style.tailHalfWidthMeters <= 0.0 ||
        !std::isfinite(style.textDepthBiasMeters) ||
        style.textDepthBiasMeters < 0.0 ||
        style.maxCharacters == 0 ||
        style.maxCharacters > kMaximumSupportedCharacters ||
        !isValidColor(style.backgroundColor) ||
        !isValidColor(style.textColor)) {
        throw std::invalid_argument("invalid speech bubble style");
    }
}

void validateRequest(const SpeechBubbleRequest& request,
                     const SpeechBubbleStyle& style) {
    if (!sim::isFinite(request.anchorMeters) || request.text.empty() ||
        request.text.find_first_not_of(' ') == std::string_view::npos ||
        request.text.size() > style.maxCharacters ||
        !std::isfinite(request.hideDistanceMeters) ||
        request.hideDistanceMeters <= 0.0) {
        throw std::invalid_argument("invalid speech bubble request");
    }
    validateBitmapFontText(request.text);
    // The background width depends on the text length, so this pairing can
    // only be checked per request. It belongs in the validation pass: a
    // distance-culled request must not decide whether the style is legal.
    if (style.tailHalfWidthMeters >
        0.5 * backgroundWidthMeters(request.text.size(), style)) {
        throw std::invalid_argument(
            "speech bubble tail is wider than its background");
    }
}

[[nodiscard]] std::uint32_t reserveVertexIndices(
    const VisiaGeometry& geometry,
    const std::uint32_t count) {
    const std::size_t maximum =
        static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max());
    if (geometry.vertices.size() > maximum - count) {
        throw std::overflow_error("speech bubble vertex index overflow");
    }
    return static_cast<std::uint32_t>(geometry.vertices.size());
}

void appendQuad(VisiaGeometry& geometry,
                const sim::Vec3 lowerLeft,
                const sim::Vec3 horizontal,
                const sim::Vec3 vertical,
                const sim::Vec3 normal,
                const std::array<float, 4>& color) {
    const std::uint32_t base = reserveVertexIndices(geometry, 4);
    appendVertex(geometry, lowerLeft, normal, color);
    appendVertex(geometry, add(lowerLeft, horizontal), normal, color);
    appendVertex(
        geometry, add(add(lowerLeft, horizontal), vertical), normal, color);
    appendVertex(geometry, add(lowerLeft, vertical), normal, color);
    geometry.indices.insert(
        geometry.indices.end(),
        {base, base + 1U, base + 2U, base, base + 2U, base + 3U});
}

void appendTail(VisiaGeometry& geometry,
                const sim::Vec3 anchor,
                const sim::Vec3 baseRight,
                const sim::Vec3 baseLeft,
                const sim::Vec3 normal,
                const std::array<float, 4>& color) {
    const std::uint32_t base = reserveVertexIndices(geometry, 3);
    appendVertex(geometry, anchor, normal, color);
    appendVertex(geometry, baseRight, normal, color);
    appendVertex(geometry, baseLeft, normal, color);
    geometry.indices.insert(
        geometry.indices.end(), {base, base + 1U, base + 2U});
}

void appendGlyph(VisiaGeometry& geometry,
                 const char character,
                 const sim::Vec3 lowerLeft,
                 const IsometricCamera& camera,
                 const sim::Vec3 normal,
                 const SpeechBubbleStyle& style) {
    const BitmapGlyphRows& rows = bitmapGlyph5x7(character);
    const sim::Vec3 pixelRight =
        scale(camera.right, style.pixelSizeMeters);
    const sim::Vec3 pixelUp = scale(camera.up, style.pixelSizeMeters);
    for (std::uint32_t row = 0; row < kBitmapGlyphHeight; ++row) {
        for (std::uint32_t column = 0;
             column < kBitmapGlyphWidth;
             ++column) {
            const std::uint8_t bit = static_cast<std::uint8_t>(
                1U << (kBitmapGlyphWidth - 1U - column));
            if ((rows[row] & bit) == 0U) {
                continue;
            }
            const double x =
                static_cast<double>(column) * style.pixelSizeMeters;
            const double y =
                static_cast<double>(kBitmapGlyphHeight - 1U - row) *
                style.pixelSizeMeters;
            appendQuad(
                geometry,
                add(add(lowerLeft, scale(camera.right, x)),
                    scale(camera.up, y)),
                pixelRight,
                pixelUp,
                normal,
                style.textColor);
        }
    }
}

[[nodiscard]] double distanceFromEye(const IsometricCamera& camera,
                                     const sim::Vec3 anchor) noexcept {
    const sim::Vec3 delta = subtract(anchor, camera.eyeMeters);
    return std::hypot(delta.x, delta.y, delta.z);
}

void appendBubble(VisiaGeometry& geometry,
                  const IsometricCamera& camera,
                  const SpeechBubbleRequest& request,
                  const SpeechBubbleStyle& style) {
    const double glyphWidth = glyphWidthMeters(style);
    const double backgroundWidth =
        backgroundWidthMeters(request.text.size(), style);
    const double backgroundHeight =
        static_cast<double>(kBitmapGlyphHeight) * style.pixelSizeMeters +
        2.0 * style.verticalPaddingMeters;
    const sim::Vec3 normal = scale(camera.forward, -1.0);
    const sim::Vec3 bottomCenter = add(
        request.anchorMeters,
        scale(camera.up, style.tailHeightMeters));
    const sim::Vec3 backgroundLowerLeft = add(
        bottomCenter,
        scale(camera.right, -0.5 * backgroundWidth));
    const sim::Vec3 tailBaseRight = add(
        bottomCenter,
        scale(camera.right, style.tailHalfWidthMeters));
    const sim::Vec3 tailBaseLeft = add(
        bottomCenter,
        scale(camera.right, -style.tailHalfWidthMeters));

    appendTail(geometry,
               request.anchorMeters,
               tailBaseRight,
               tailBaseLeft,
               normal,
               style.backgroundColor);
    appendQuad(
        geometry,
        backgroundLowerLeft,
        scale(camera.right, backgroundWidth),
        scale(camera.up, backgroundHeight),
        normal,
        style.backgroundColor);

    sim::Vec3 glyphLowerLeft = add(
        add(backgroundLowerLeft,
            scale(camera.right, style.horizontalPaddingMeters)),
        scale(camera.up, style.verticalPaddingMeters));
    glyphLowerLeft = add(
        glyphLowerLeft,
        scale(normal, style.textDepthBiasMeters));
    const double advance = glyphWidth + style.glyphSpacingMeters;
    for (std::size_t index = 0; index < request.text.size(); ++index) {
        appendGlyph(
            geometry,
            request.text[index],
            add(glyphLowerLeft,
                scale(camera.right,
                      static_cast<double>(index) * advance)),
            camera,
            normal,
            style);
    }
}

}  // namespace

// @implements spec/feature/npc-conversations-and-placement-feedback.md Conversations
VisiaGeometry buildSpeechBubbleGeometry(
    const IsometricCamera& camera,
    const std::span<const SpeechBubbleRequest> requests,
    const SpeechBubbleStyle& style) {
    validateCamera(camera);
    validateStyle(style);
    for (const SpeechBubbleRequest& request : requests) {
        validateRequest(request, style);
    }

    VisiaGeometry geometry;
    for (const SpeechBubbleRequest& request : requests) {
        if (distanceFromEye(camera, request.anchorMeters) >
            request.hideDistanceMeters) {
            continue;
        }
        appendBubble(geometry, camera, request, style);
    }
    return geometry;
}

}  // namespace konbini::render
