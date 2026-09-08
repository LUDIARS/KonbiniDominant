#include "konbini/render/speech_bubble_geometry.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "konbini/render/bitmap_font.h"
#include "konbini/render/isometric_camera.h"

#include "../check.h"

// @implements spec/test/verification-strategy.md 1. Data / ID unit tests
// @implements spec/interface/visia-presentation.md Speech bubble geometry
// @implements spec/feature/npc-conversations-and-placement-feedback.md Conversations

namespace {

using konbini::render::IsometricCamera;
using konbini::render::IsometricCameraConfig;
using konbini::render::SpeechBubbleRequest;
using konbini::render::SpeechBubbleStyle;
using konbini::render::ViewportExtent;
using konbini::render::VisiaGeometry;
using konbini::render::bitmapGlyph5x7;
using konbini::render::buildIsometricCamera;
using konbini::render::buildSpeechBubbleGeometry;
using konbini::render::isBitmapFontCharacter;
using konbini::render::kBitmapGlyphHeight;
using konbini::render::kBitmapGlyphWidth;
using konbini::render::validateBitmapFontText;
using konbini::sim::Vec3;

// One tail triangle plus one background quad, before any glyph pixel.
constexpr std::size_t kChromeVertices = 3 + 4;
constexpr std::size_t kChromeIndices = 3 + 6;
constexpr std::size_t kVerticesPerPixel = 4;
constexpr std::size_t kIndicesPerPixel = 6;

[[nodiscard]] IsometricCamera makeCamera() {
    return buildIsometricCamera(IsometricCameraConfig{}, ViewportExtent{800, 600});
}

// Mirrors the emitter: one quad per lit pixel of the 5x7 glyph rows.
[[nodiscard]] std::size_t litPixelCount(const std::string_view text) {
    std::size_t count = 0;
    for (const char character : text) {
        const auto& rows = bitmapGlyph5x7(character);
        for (const std::uint8_t row : rows) {
            for (std::uint32_t column = 0; column < kBitmapGlyphWidth;
                 ++column) {
                const std::uint8_t bit = static_cast<std::uint8_t>(
                    1U << (kBitmapGlyphWidth - 1U - column));
                if ((row & bit) != 0U) {
                    ++count;
                }
            }
        }
    }
    return count;
}

[[nodiscard]] double distanceFromEye(const IsometricCamera& camera,
                                     const Vec3 anchor) {
    return std::hypot(anchor.x - camera.eyeMeters.x,
                      anchor.y - camera.eyeMeters.y,
                      anchor.z - camera.eyeMeters.z);
}

[[nodiscard]] SpeechBubbleRequest makeRequest(const IsometricCamera& camera,
                                              const std::string_view text) {
    const Vec3 anchor{camera.eyeMeters.x + 10.0, camera.eyeMeters.y,
                      camera.eyeMeters.z};
    return {
        .anchorMeters = anchor,
        .text = text,
        .hideDistanceMeters = distanceFromEye(camera, anchor) + 1.0,
    };
}

[[nodiscard]] bool everyIndexIsInRange(const VisiaGeometry& geometry) {
    for (const std::uint32_t index : geometry.indices) {
        if (static_cast<std::size_t>(index) >= geometry.vertices.size()) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool everyVertexIsFinite(const VisiaGeometry& geometry) {
    for (const auto& vertex : geometry.vertices) {
        for (const float value : vertex.position) {
            if (!std::isfinite(value)) {
                return false;
            }
        }
        for (const float value : vertex.normal) {
            if (!std::isfinite(value)) {
                return false;
            }
        }
    }
    return true;
}

// --- supported glyphs --------------------------------------------------

void testEverySupportedGlyphIsAvailable() {
    for (char character = 'A'; character <= 'Z'; ++character) {
        CHECK_NO_THROW((void)bitmapGlyph5x7(character));
        const auto& rows = bitmapGlyph5x7(character);
        CHECK(rows.size() == kBitmapGlyphHeight);
        std::uint8_t combined = 0;
        for (const std::uint8_t row : rows) {
            combined = static_cast<std::uint8_t>(combined | row);
            // A glyph row only owns the low five bits; a wider row would
            // bleed into the neighbouring character cell.
            CHECK((row & static_cast<std::uint8_t>(0xE0U)) == 0U);
        }
        // Every letter has to draw something.
        CHECK(combined != 0U);
    }

    // Digits and the HUD punctuation share the same cell as the letters; the
    // HUD prints cash, store counts and tick numbers with them.
    for (char character = '0'; character <= '9'; ++character) {
        CHECK_NO_THROW((void)bitmapGlyph5x7(character));
        const auto& rows = bitmapGlyph5x7(character);
        std::uint8_t combined = 0;
        for (const std::uint8_t row : rows) {
            combined = static_cast<std::uint8_t>(combined | row);
            CHECK((row & static_cast<std::uint8_t>(0xE0U)) == 0U);
        }
        CHECK(combined != 0U);
    }
    for (const char character : {'-', ':', '/', '.', '+'}) {
        CHECK_NO_THROW((void)bitmapGlyph5x7(character));
        for (const std::uint8_t row : bitmapGlyph5x7(character)) {
            CHECK((row & static_cast<std::uint8_t>(0xE0U)) == 0U);
        }
    }

    // Space is the one blank glyph, and it is a supported character.
    CHECK_NO_THROW((void)bitmapGlyph5x7(' '));
    for (const std::uint8_t row : bitmapGlyph5x7(' ')) {
        CHECK(row == 0U);
    }

    // isBitmapFontCharacter is the non-throwing probe for the same
    // repertoire, so the two must never disagree.
    for (int value = 0; value < 128; ++value) {
        const char character = static_cast<char>(value);
        CHECK(isBitmapFontCharacter(character) ==
              konbini::test::doesNotThrow(
                  [&] { (void)bitmapGlyph5x7(character); }));
    }

    CHECK_NO_THROW(validateBitmapFontText("HANDY LOCATION"));
    CHECK_NO_THROW(validateBitmapFontText("CASH 1200 +5.5"));
    CHECK_NO_THROW(validateBitmapFontText(""));
}

void testUnsupportedCharactersAreRejected() {
    // Lower case, unsupported punctuation and control bytes all have to fail
    // rather than resolve to a missing-glyph box.
    for (const char character :
         {'a', 'z', '!', '@', '#', '?', ',', '\t', '\n'}) {
        CHECK_THROWS(std::invalid_argument, (void)bitmapGlyph5x7(character));
    }
    // Just outside the letter range on either side.
    CHECK_THROWS(std::invalid_argument,
                 (void)bitmapGlyph5x7(static_cast<char>('A' - 1)));
    CHECK_THROWS(std::invalid_argument,
                 (void)bitmapGlyph5x7(static_cast<char>('Z' + 1)));

    CHECK_THROWS(std::invalid_argument, validateBitmapFontText("Nice"));
    CHECK_THROWS(std::invalid_argument, validateBitmapFontText("STORE, 24H"));

    const IsometricCamera camera = makeCamera();
    const SpeechBubbleStyle style;
    const std::array<SpeechBubbleRequest, 1> lowercase{
        makeRequest(camera, "nice")};
    CHECK_THROWS(std::invalid_argument,
                 (void)buildSpeechBubbleGeometry(camera, lowercase, style));
}

// --- emitted geometry --------------------------------------------------

void testBubbleEmitsChromeAndOneQuadPerLitPixel() {
    const IsometricCamera camera = makeCamera();
    const SpeechBubbleStyle style;

    for (const std::string_view text :
         {std::string_view("A"), std::string_view("HANDY LOCATION"),
          std::string_view("A ")}) {
        const std::array<SpeechBubbleRequest, 1> requests{
            makeRequest(camera, text)};
        const VisiaGeometry geometry =
            buildSpeechBubbleGeometry(camera, requests, style);

        const std::size_t pixels = litPixelCount(text);
        CHECK(geometry.vertices.size() ==
              kChromeVertices + (kVerticesPerPixel * pixels));
        CHECK(geometry.indices.size() ==
              kChromeIndices + (kIndicesPerPixel * pixels));
        CHECK(everyIndexIsInRange(geometry));
        CHECK(everyVertexIsFinite(geometry));
    }

    // A trailing space widens the bubble but draws no extra pixel.
    const std::array<SpeechBubbleRequest, 1> tight{makeRequest(camera, "A")};
    const std::array<SpeechBubbleRequest, 1> padded{makeRequest(camera, "A ")};
    CHECK(buildSpeechBubbleGeometry(camera, tight, style).vertices.size() ==
          buildSpeechBubbleGeometry(camera, padded, style).vertices.size());
}

void testEmptyRequestListEmitsNothing() {
    const IsometricCamera camera = makeCamera();
    const SpeechBubbleStyle style;
    const std::vector<SpeechBubbleRequest> requests;
    const VisiaGeometry geometry =
        buildSpeechBubbleGeometry(camera, requests, style);
    CHECK(geometry.vertices.empty());
    CHECK(geometry.indices.empty());
}

// Index space stays inside the 32-bit index type. A true overflow would need
// billions of vertices, so what is pinned here is the bound that prevents it:
// the supported character cap keeps one bubble's index range small, and every
// emitted index still addresses a vertex this bubble owns.
void testIndexSpaceStaysWithinItsBudget() {
    const IsometricCamera camera = makeCamera();
    SpeechBubbleStyle style;
    style.maxCharacters = 64;

    const std::string text(64, 'M');
    const std::array<SpeechBubbleRequest, 1> requests{
        makeRequest(camera, text)};
    const VisiaGeometry geometry =
        buildSpeechBubbleGeometry(camera, requests, style);

    const std::size_t pixels = litPixelCount(text);
    CHECK(geometry.vertices.size() ==
          kChromeVertices + (kVerticesPerPixel * pixels));
    CHECK(everyIndexIsInRange(geometry));
    CHECK(geometry.vertices.size() <
          static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()));

    // Many bubbles in one geometry keep addressing their own vertices.
    const std::array<SpeechBubbleRequest, 3> several{
        makeRequest(camera, "ALPHA"), makeRequest(camera, "BETA"),
        makeRequest(camera, "GAMMA")};
    const VisiaGeometry combined =
        buildSpeechBubbleGeometry(camera, several, style);
    CHECK(everyIndexIsInRange(combined));
    CHECK(combined.vertices.size() ==
          (3 * kChromeVertices) +
              (kVerticesPerPixel *
               (litPixelCount("ALPHA") + litPixelCount("BETA") +
                litPixelCount("GAMMA"))));

    // A style above the supported cap is refused instead of silently
    // truncating the line.
    SpeechBubbleStyle tooManyCharacters;
    tooManyCharacters.maxCharacters = 65;
    const std::array<SpeechBubbleRequest, 1> shortRequest{
        makeRequest(camera, "A")};
    CHECK_THROWS(std::invalid_argument,
                 (void)buildSpeechBubbleGeometry(camera, shortRequest,
                                                 tooManyCharacters));
}

// --- distance culling --------------------------------------------------

// The hide distance is inclusive: a bubble exactly at its limit is still
// drawn, and only a request beyond it is dropped.
void testDistanceBoundaryIsInclusive() {
    const IsometricCamera camera = makeCamera();
    const SpeechBubbleStyle style;
    const Vec3 anchor{camera.eyeMeters.x + 10.0, camera.eyeMeters.y,
                      camera.eyeMeters.z};
    const double distance = distanceFromEye(camera, anchor);

    const std::array<SpeechBubbleRequest, 1> atLimit{SpeechBubbleRequest{
        .anchorMeters = anchor,
        .text = "ALPHA",
        .hideDistanceMeters = distance,
    }};
    CHECK(!buildSpeechBubbleGeometry(camera, atLimit, style).vertices.empty());

    const std::array<SpeechBubbleRequest, 1> justBeyond{SpeechBubbleRequest{
        .anchorMeters = anchor,
        .text = "ALPHA",
        .hideDistanceMeters = std::nextafter(distance, 0.0),
    }};
    CHECK(buildSpeechBubbleGeometry(camera, justBeyond, style)
              .vertices.empty());

    // A culled request is still validated: only its geometry is skipped.
    const std::array<SpeechBubbleRequest, 1> culledButInvalid{
        SpeechBubbleRequest{
            .anchorMeters = anchor,
            .text = "alpha",
            .hideDistanceMeters = std::nextafter(distance, 0.0),
        }};
    CHECK_THROWS(std::invalid_argument,
                 (void)buildSpeechBubbleGeometry(camera, culledButInvalid,
                                                 style));

    // Culling one request must not remove the others.
    const std::array<SpeechBubbleRequest, 2> mixed{
        SpeechBubbleRequest{
            .anchorMeters = anchor,
            .text = "ALPHA",
            .hideDistanceMeters = std::nextafter(distance, 0.0),
        },
        SpeechBubbleRequest{
            .anchorMeters = anchor,
            .text = "ALPHA",
            .hideDistanceMeters = distance,
        },
    };
    const VisiaGeometry mixedGeometry =
        buildSpeechBubbleGeometry(camera, mixed, style);
    CHECK(mixedGeometry.vertices.size() ==
          kChromeVertices + (kVerticesPerPixel * litPixelCount("ALPHA")));
}

// --- camera basis ------------------------------------------------------

// Bubbles are built directly on the camera basis, so a degenerate or mirrored
// basis would silently flip the text instead of failing.
void testCameraBasisIsValidated() {
    const IsometricCamera camera = makeCamera();
    const SpeechBubbleStyle style;
    const std::array<SpeechBubbleRequest, 1> requests{
        makeRequest(camera, "ALPHA")};

    CHECK_NO_THROW((void)buildSpeechBubbleGeometry(camera, requests, style));

    IsometricCamera scaledForward = camera;
    scaledForward.forward.x *= 2.0;
    scaledForward.forward.y *= 2.0;
    scaledForward.forward.z *= 2.0;
    CHECK_THROWS(std::invalid_argument,
                 (void)buildSpeechBubbleGeometry(scaledForward, requests,
                                                 style));

    IsometricCamera nonOrthogonal = camera;
    nonOrthogonal.right = camera.up;
    CHECK_THROWS(std::invalid_argument,
                 (void)buildSpeechBubbleGeometry(nonOrthogonal, requests,
                                                 style));

    // Mirroring the basis keeps it orthonormal but reverses handedness, which
    // would draw every line backwards.
    IsometricCamera mirrored = camera;
    mirrored.right.x = -camera.right.x;
    mirrored.right.y = -camera.right.y;
    mirrored.right.z = -camera.right.z;
    CHECK_THROWS(std::invalid_argument,
                 (void)buildSpeechBubbleGeometry(mirrored, requests, style));

    IsometricCamera zeroExtent = camera;
    zeroExtent.extent.width = 0;
    CHECK_THROWS(std::invalid_argument,
                 (void)buildSpeechBubbleGeometry(zeroExtent, requests, style));

    IsometricCamera zeroHeight = camera;
    zeroHeight.extent.height = 0;
    CHECK_THROWS(std::invalid_argument,
                 (void)buildSpeechBubbleGeometry(zeroHeight, requests, style));

    IsometricCamera nonFiniteEye = camera;
    nonFiniteEye.eyeMeters.x = std::numeric_limits<double>::quiet_NaN();
    CHECK_THROWS(std::invalid_argument,
                 (void)buildSpeechBubbleGeometry(nonFiniteEye, requests,
                                                 style));

    IsometricCamera zeroBasis = camera;
    zeroBasis.up = Vec3{0.0, 0.0, 0.0};
    CHECK_THROWS(std::invalid_argument,
                 (void)buildSpeechBubbleGeometry(zeroBasis, requests, style));
}

// --- style and request validation --------------------------------------

void testStyleIsValidated() {
    const IsometricCamera camera = makeCamera();
    const std::array<SpeechBubbleRequest, 1> requests{
        makeRequest(camera, "ALPHA")};

    const auto rejects = [&](const SpeechBubbleStyle& style) {
        CHECK_THROWS(std::invalid_argument,
                     (void)buildSpeechBubbleGeometry(camera, requests, style));
    };

    SpeechBubbleStyle style;
    style.pixelSizeMeters = 0.0;
    rejects(style);

    style = SpeechBubbleStyle{};
    style.glyphSpacingMeters = -0.01;
    rejects(style);

    style = SpeechBubbleStyle{};
    style.horizontalPaddingMeters = 0.0;
    rejects(style);

    style = SpeechBubbleStyle{};
    style.verticalPaddingMeters = 0.0;
    rejects(style);

    style = SpeechBubbleStyle{};
    style.tailHeightMeters = 0.0;
    rejects(style);

    style = SpeechBubbleStyle{};
    style.tailHalfWidthMeters = 0.0;
    rejects(style);

    style = SpeechBubbleStyle{};
    style.textDepthBiasMeters = -0.01;
    rejects(style);

    style = SpeechBubbleStyle{};
    style.maxCharacters = 0;
    rejects(style);

    style = SpeechBubbleStyle{};
    style.backgroundColor[3] = 0.0F;
    rejects(style);

    style = SpeechBubbleStyle{};
    style.textColor[0] = 1.5F;
    rejects(style);

    style = SpeechBubbleStyle{};
    style.pixelSizeMeters = std::numeric_limits<double>::quiet_NaN();
    rejects(style);

    // A zero spacing is legal: glyphs may sit flush against each other.
    style = SpeechBubbleStyle{};
    style.glyphSpacingMeters = 0.0;
    CHECK_NO_THROW(
        (void)buildSpeechBubbleGeometry(camera, requests, style));
}

void testRequestIsValidated() {
    const IsometricCamera camera = makeCamera();
    const SpeechBubbleStyle style;
    const Vec3 anchor{camera.eyeMeters.x + 10.0, camera.eyeMeters.y,
                      camera.eyeMeters.z};
    const double hideDistance = distanceFromEye(camera, anchor) + 1.0;

    const auto rejects = [&](const SpeechBubbleRequest& request) {
        const std::array<SpeechBubbleRequest, 1> requests{request};
        CHECK_THROWS(std::invalid_argument,
                     (void)buildSpeechBubbleGeometry(camera, requests, style));
    };

    rejects({.anchorMeters = anchor,
             .text = "",
             .hideDistanceMeters = hideDistance});
    // Whitespace draws nothing, so an all-space line is an empty line.
    rejects({.anchorMeters = anchor,
             .text = "   ",
             .hideDistanceMeters = hideDistance});
    rejects({.anchorMeters = {std::numeric_limits<double>::quiet_NaN(), 0.0,
                              0.0},
             .text = "ALPHA",
             .hideDistanceMeters = hideDistance});
    rejects({.anchorMeters = anchor,
             .text = "ALPHA",
             .hideDistanceMeters = 0.0});
    rejects({.anchorMeters = anchor,
             .text = "ALPHA",
             .hideDistanceMeters = -1.0});
    rejects({.anchorMeters = anchor,
             .text = "ALPHA",
             .hideDistanceMeters =
                 std::numeric_limits<double>::infinity()});

    // The default style carries a 24 character budget.
    const std::string atLimit(style.maxCharacters, 'A');
    const std::array<SpeechBubbleRequest, 1> atLimitRequests{
        SpeechBubbleRequest{.anchorMeters = anchor,
                            .text = atLimit,
                            .hideDistanceMeters = hideDistance}};
    CHECK_NO_THROW(
        (void)buildSpeechBubbleGeometry(camera, atLimitRequests, style));

    const std::string overLimit(style.maxCharacters + 1, 'A');
    rejects({.anchorMeters = anchor,
             .text = overLimit,
             .hideDistanceMeters = hideDistance});
}

// A tail wider than the bubble it hangs from would stick out on both sides.
// The check depends on the text length, so it belongs to the request.
void testTailMustFitItsBackground() {
    const IsometricCamera camera = makeCamera();
    SpeechBubbleStyle style;
    const Vec3 anchor{camera.eyeMeters.x + 10.0, camera.eyeMeters.y,
                      camera.eyeMeters.z};
    const double hideDistance = distanceFromEye(camera, anchor) + 1.0;
    const double singleGlyphWidth =
        static_cast<double>(kBitmapGlyphWidth) * style.pixelSizeMeters;
    const double backgroundWidth =
        singleGlyphWidth + (2.0 * style.horizontalPaddingMeters);

    style.tailHalfWidthMeters = 0.5 * backgroundWidth;
    const std::array<SpeechBubbleRequest, 1> requests{SpeechBubbleRequest{
        .anchorMeters = anchor,
        .text = "A",
        .hideDistanceMeters = hideDistance}};
    CHECK_NO_THROW((void)buildSpeechBubbleGeometry(camera, requests, style));

    style.tailHalfWidthMeters = (0.5 * backgroundWidth) + 0.01;
    CHECK_THROWS(std::invalid_argument,
                 (void)buildSpeechBubbleGeometry(camera, requests, style));

    // The same tail is legal once the line is long enough to carry it.
    const std::array<SpeechBubbleRequest, 1> longerRequests{
        SpeechBubbleRequest{.anchorMeters = anchor,
                            .text = "ALPHA",
                            .hideDistanceMeters = hideDistance}};
    CHECK_NO_THROW(
        (void)buildSpeechBubbleGeometry(camera, longerRequests, style));
}

}  // namespace

int main() {
    testEverySupportedGlyphIsAvailable();
    testUnsupportedCharactersAreRejected();
    testBubbleEmitsChromeAndOneQuadPerLitPixel();
    testEmptyRequestListEmitsNothing();
    testIndexSpaceStaysWithinItsBudget();
    testDistanceBoundaryIsInclusive();
    testCameraBasisIsValidated();
    testStyleIsValidated();
    testRequestIsValidated();
    testTailMustFitItsBackground();
    return konbini::test::summarize("speech_bubble_geometry_test");
}
