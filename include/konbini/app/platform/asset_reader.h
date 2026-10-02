#pragma once

#include <cstddef>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

// @implements spec/interface/mobile-platform.md Assets and generated geometry

namespace konbini::app {

class AssetNotFound : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Read-only packaged assets addressed by package-relative name. Android
// AAssetManager and an iOS bundle need no extracted filesystem path, so
// loaders take bytes from here instead of an absolute path.
// @implements spec/interface/mobile-platform.md Assets and generated geometry
class IAssetReader {
public:
    virtual ~IAssetReader() = default;

    // Both validate `name` with `validateRelativeResourceName`.
    [[nodiscard]] virtual bool contains(std::string_view name) const = 0;
    // Throws `AssetNotFound` for a missing asset.
    [[nodiscard]] virtual std::vector<std::byte> read(
        std::string_view name) const = 0;
};

// Boot check. Throws `AssetNotFound` naming every missing asset at once.
// @implements spec/interface/mobile-platform.md Failure policy
void requireAssets(
    const IAssetReader& reader, std::span<const std::string_view> names);

}  // namespace konbini::app
