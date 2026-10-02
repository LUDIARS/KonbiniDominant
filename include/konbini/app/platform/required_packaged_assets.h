#pragma once

#include <string_view>
#include <vector>

// @implements spec/interface/mobile-platform.md Failure policy

namespace konbini::app {

// Package-relative names the runtime cannot start without: the content file
// and every SPIR-V module of `kRequiredShaderFiles`.
// @implements spec/interface/mobile-platform.md Failure policy
[[nodiscard]] std::vector<std::string_view> requiredPackagedAssets();

}  // namespace konbini::app
