#pragma once

#include <filesystem>
#include <span>
#include <string_view>

#include "konbini/app/platform/asset_reader.h"

// @implements spec/interface/mobile-platform.md Assets and generated geometry

namespace konbini::app {

// Copies packaged assets into `root` for loaders that still take a
// filesystem path (content file, shader directory). The reader stays the
// source: every listed asset is checked first and all missing names are
// reported at once (`AssetNotFound`) before any file is written. Each file is
// written next to its target and renamed into place, so an interrupted boot
// never leaves a truncated shader the next boot would load. `root` must be
// absolute (`std::invalid_argument`); a write failure is `std::runtime_error`.
// @implements spec/interface/mobile-platform.md Assets and generated geometry
void materializePackagedAssets(const IAssetReader& reader,
                               std::span<const std::string_view> names,
                               const std::filesystem::path& root);

}  // namespace konbini::app
