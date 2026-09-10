// @implements spec/feature/grid-town-and-vector-ui.md Vector font
#include "konbini/render/vector_font.h"
#include <stdexcept>

namespace konbini::render {
namespace {
struct GlyphRange {
    std::uint32_t firstPoint, pointCount, firstIndex, indexCount;
    float advance;
};
#include "generated/roboto_mono_mesh.inc"
}
VectorGlyph vectorGlyph(const char character) {
    const auto code = static_cast<unsigned char>(character);
    if (code < 32 || code > 126) throw std::invalid_argument("vector font character is outside the baked ASCII set");
    const auto& range = kFontGlyphs[code-32];
    return {{kFontPoints+range.firstPoint,range.pointCount},
            {kFontIndices+range.firstIndex,range.indexCount},range.advance};
}
void validateVectorFontText(std::string_view text) {
    for (const char character : text) (void)vectorGlyph(character);
}
}
