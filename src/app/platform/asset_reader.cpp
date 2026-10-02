#include "konbini/app/platform/asset_reader.h"

#include <string>

// @implements spec/interface/mobile-platform.md Failure policy

namespace konbini::app {

// @implements spec/interface/mobile-platform.md Failure policy
void requireAssets(
    const IAssetReader& reader, const std::span<const std::string_view> names) {
    std::string missing;
    for (const std::string_view name : names) {
        if (reader.contains(name)) {
            continue;
        }
        missing += missing.empty() ? "" : ", ";
        missing += name;
    }
    if (!missing.empty()) {
        throw AssetNotFound("required packaged asset(s) missing: " + missing);
    }
}

}  // namespace konbini::app
