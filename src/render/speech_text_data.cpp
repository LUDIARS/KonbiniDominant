#include "speech_text_data.h"

// @implements spec/feature/npc-conversations-and-placement-feedback.md Japanese speech lines

namespace konbini::render::detail {

namespace {
#include "generated/noto_sans_jp_speech_mesh.inc"
}  // namespace

const char* speechDataLocale() noexcept {
    return kSpeechLocale;
}

std::span<const SpeechGlyphPoint> speechGlyphPoints() noexcept {
    return kSpeechGlyphPoints;
}

std::span<const std::uint16_t> speechGlyphIndices() noexcept {
    return kSpeechGlyphIndices;
}

std::span<const SpeechGlyphRange> speechGlyphRanges() noexcept {
    return kSpeechGlyphRanges;
}

std::span<const SpeechLineEntry> speechLineEntries() noexcept {
    return kSpeechLineEntries;
}

}  // namespace konbini::render::detail
