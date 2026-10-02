#pragma once

#include <span>
#include <string>
#include <string_view>

// @implements spec/feature/npc-conversations-and-placement-feedback.md Japanese speech lines

namespace konbini::render {

// One localized speech line. `key` is the localization key the simulation
// emits (`ResidentPresentation::speech`, i.e. a `residentPresentation.remarks`
// entry); `text` is the UTF-8 line shown in the bubble.
struct SpeechLine {
    std::string_view key;
    std::string_view text;
};

// Locale of the baked catalog (data/locale/<locale>/resident_remarks.json).
[[nodiscard]] std::string_view speechLocale() noexcept;
[[nodiscard]] std::span<const SpeechLine> speechLines() noexcept;

// Unknown keys are `std::out_of_range`: a missing translation is never shown
// as the raw key or as an empty bubble.
[[nodiscard]] const SpeechLine& speechLine(std::string_view key);

// Startup check that every content remark resolves to a line whose glyphs are
// all in the atlas. Throws the first failure.
void validateSpeechLineKeys(std::span<const std::string> keys);

}  // namespace konbini::render
