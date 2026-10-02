#pragma once

#include <string_view>

// @implements spec/interface/mobile-platform.md Assets and generated geometry

namespace konbini::app {

// Package- or root-relative name such as `shaders/konbini_world.vert.spv`.
// Rejects empty names, absolute paths, drive letters, backslashes, empty
// segments, and `.` / `..` segments with `std::invalid_argument`, so a name
// can neither escape its root nor smuggle a host filesystem path in.
// @implements spec/interface/mobile-platform.md Assets and generated geometry
void validateRelativeResourceName(std::string_view name);

}  // namespace konbini::app
