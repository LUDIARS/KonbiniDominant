#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include "konbini/sim/entity_id.h"
#include "konbini/sim/resident_presentation.h"

// @implements spec/interface/pictor-rendering.md Presentation objects

namespace konbini::render {

// Packs an entity id into an object-key owner: index in the high half,
// generation in the low half, so ids that differ only by generation never
// share an object.
[[nodiscard]] constexpr std::uint64_t packEntityKey(
    const sim::EntityId id) noexcept {
    return (static_cast<std::uint64_t>(id.index) << 32U) | id.generation;
}

[[nodiscard]] constexpr std::uint64_t residentOwnerKey(
    const sim::ResidentPresentationId& id) noexcept {
    return packEntityKey(id.populationCellId.value());
}

// Resident ordinal in the high bits, glyph position in the low byte.
[[nodiscard]] inline std::uint64_t speechGlyphPart(
    const sim::ResidentPresentationId& id, const std::size_t position) {
    if (position > 0xFFU) {
        throw std::out_of_range("speech glyph position exceeds the key byte");
    }
    return (static_cast<std::uint64_t>(id.ordinal) << 8U) | position;
}

}  // namespace konbini::render
