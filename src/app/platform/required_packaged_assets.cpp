#include "konbini/app/platform/required_packaged_assets.h"

#include <array>
#include <string>

#include "konbini/app/app_paths.h"

// @implements spec/interface/mobile-platform.md Failure policy

namespace konbini::app {
namespace {

// Same layout `StageRuntimeAssets.cmake` stages next to the executable and
// the mobile packages copy into their asset bundle.
constexpr std::string_view kContentAsset = "data/content/first-playable.json";

// Shader names are prefixed once and kept for the process lifetime, so the
// returned views stay valid.
[[nodiscard]] const std::array<std::string, kRequiredShaderFiles.size()>&
shaderAssetNames() {
    static const auto names = [] {
        std::array<std::string, kRequiredShaderFiles.size()> prefixed;
        for (std::size_t index = 0; index < kRequiredShaderFiles.size(); ++index) {
            prefixed[index] = "shaders/" + std::string(kRequiredShaderFiles[index]);
        }
        return prefixed;
    }();
    return names;
}

}  // namespace

// @implements spec/interface/mobile-platform.md Failure policy
std::vector<std::string_view> requiredPackagedAssets() {
    std::vector<std::string_view> names;
    names.reserve(1 + kRequiredShaderFiles.size());
    names.push_back(kContentAsset);
    for (const std::string& shader : shaderAssetNames()) {
        names.push_back(shader);
    }
    return names;
}

}  // namespace konbini::app
