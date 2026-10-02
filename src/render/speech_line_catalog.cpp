#include "konbini/render/speech_line_catalog.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "konbini/render/speech_glyph_atlas.h"
#include "speech_text_data.h"

// @implements spec/feature/npc-conversations-and-placement-feedback.md Japanese speech lines

namespace konbini::render {

namespace {

[[nodiscard]] const std::vector<SpeechLine>& lineTable() {
    static const std::vector<SpeechLine> table = [] {
        std::vector<SpeechLine> lines;
        for (const detail::SpeechLineEntry& entry :
             detail::speechLineEntries()) {
            lines.push_back({.key = entry.key, .text = entry.text});
        }
        return lines;
    }();
    return table;
}

}  // namespace

std::string_view speechLocale() noexcept {
    return detail::speechDataLocale();
}

std::span<const SpeechLine> speechLines() noexcept {
    return lineTable();
}

const SpeechLine& speechLine(const std::string_view key) {
    const std::vector<SpeechLine>& table = lineTable();
    const auto found = std::find_if(
        table.begin(), table.end(),
        [key](const SpeechLine& line) { return line.key == key; });
    if (found == table.end()) {
        throw std::out_of_range(
            "speech line catalog has no line for this localization key");
    }
    return *found;
}

// @implements spec/feature/npc-conversations-and-placement-feedback.md Japanese speech lines
void validateSpeechLineKeys(const std::span<const std::string> keys) {
    for (const std::string& key : keys) {
        const SpeechLine& line = speechLine(key);
        for (const char32_t codepoint : decodeUtf8(line.text)) {
            static_cast<void>(speechGlyph(codepoint));
        }
    }
}

}  // namespace konbini::render
