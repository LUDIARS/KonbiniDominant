#pragma once

#include <array>
#include <compare>
#include <cstdint>

#include "konbini/render/world_palette.h"

// @implements spec/interface/pictor-rendering.md Presentation objects
// @implements spec/interface/visia-presentation.md Pictor integration boundary

namespace konbini::render {

// Shared presentation meshes. Each one is uploaded once at startup and every
// object that shows it references the same GPU mesh (resident instancing).
enum class PresentationMeshKind : std::uint8_t {
    ResidentBody = 1,
    // `index` is the baked age frame of the landing ring.
    LandingRingFrame,
    BubbleBackground,
    BubbleTail,
    // `index` is the Unicode code point of a speech glyph.
    SpeechGlyph,
};

struct PresentationMeshKey {
    PresentationMeshKind kind = PresentationMeshKind::ResidentBody;
    std::uint32_t index = 0;

    auto operator<=>(const PresentationMeshKey&) const = default;
};

// What a presentation object stands for. The owner / part pair is a stable
// game-side identity, so the same resident keeps the same Pictor object for
// as long as it stays in the snapshot.
enum class PresentationObjectRole : std::uint8_t {
    // owner = population cell id, part = resident ordinal.
    Resident = 1,
    // owner = StoreId raw value, part = 0.
    LandingEffect,
    // owner / part as Resident; the bubble background.
    BubbleBackground,
    // owner / part as Resident; the bubble tail.
    BubbleTail,
    // owner = population cell id, part = (ordinal << 8) | glyph position.
    SpeechGlyph,
};

struct PresentationObjectKey {
    PresentationObjectRole role = PresentationObjectRole::Resident;
    std::uint64_t owner = 0;
    std::uint64_t part = 0;

    auto operator<=>(const PresentationObjectKey&) const = default;
};

// One object the frame wants drawn from a shared presentation mesh.
// `model` is column-major (index = column * 4 + row), rotation / scale +
// translation only, in the same convention as `IsometricCamera`.
struct PresentationDraw {
    PresentationObjectKey key{};
    PresentationMeshKey mesh{};
    std::array<float, 16> model{};
    WorldColor tint{1.0F, 1.0F, 1.0F, 1.0F};
    // Depth-read-only blended pass (fading landing ring).
    bool translucent = false;
};

}  // namespace konbini::render
