#include "konbini/render/bitmap_font.h"

#include <stdexcept>

// @implements spec/interface/visia-presentation.md Speech bubble geometry
// @implements spec/feature/npc-conversations-and-placement-feedback.md Conversations

namespace konbini::render {

namespace {

constexpr BitmapGlyphRows kSpace{};

// Each row uses its low five bits; bit 4 is the left-most pixel.
constexpr std::array<BitmapGlyphRows, 26> kUppercaseGlyphs{{
    {{0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}},  // A
    {{0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110}},  // B
    {{0b01111, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b01111}},  // C
    {{0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110}},  // D
    {{0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111}},  // E
    {{0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000}},  // F
    {{0b01111, 0b10000, 0b10000, 0b10111, 0b10001, 0b10001, 0b01110}},  // G
    {{0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}},  // H
    {{0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b11111}},  // I
    {{0b00111, 0b00010, 0b00010, 0b00010, 0b10010, 0b10010, 0b01100}},  // J
    {{0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001}},  // K
    {{0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111}},  // L
    {{0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001}},  // M
    {{0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001, 0b10001}},  // N
    {{0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}},  // O
    {{0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000}},  // P
    {{0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101}},  // Q
    {{0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001}},  // R
    {{0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110}},  // S
    {{0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100}},  // T
    {{0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}},  // U
    {{0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100}},  // V
    {{0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b10101, 0b01010}},  // W
    {{0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001}},  // X
    {{0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100}},  // Y
    {{0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111}},  // Z
}};

}  // namespace

// @implements spec/interface/visia-presentation.md Speech bubble geometry
const BitmapGlyphRows& bitmapGlyph5x7(const char character) {
    if (character == ' ') {
        return kSpace;
    }
    if (character < 'A' || character > 'Z') {
        throw std::invalid_argument(
            "5x7 bitmap font accepts only ASCII A-Z and space");
    }
    return kUppercaseGlyphs[static_cast<std::size_t>(character - 'A')];
}

// @implements spec/feature/npc-conversations-and-placement-feedback.md Conversations
void validateBitmapFontText(const std::string_view text) {
    for (const char character : text) {
        (void)bitmapGlyph5x7(character);
    }
}

}  // namespace konbini::render
