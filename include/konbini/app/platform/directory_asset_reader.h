#pragma once

#include <filesystem>

#include "konbini/app/platform/asset_reader.h"

// @implements spec/interface/mobile-platform.md Assets and generated geometry

namespace konbini::app {

// Desktop host's reader over the staged asset directory next to the
// executable. The root is the host's business; callers only see names.
// @implements spec/interface/mobile-platform.md Assets and generated geometry
class DirectoryAssetReader final : public IAssetReader {
public:
    // Throws `AssetNotFound` when `root` is not an existing directory.
    explicit DirectoryAssetReader(std::filesystem::path root);

    [[nodiscard]] bool contains(std::string_view name) const override;
    [[nodiscard]] std::vector<std::byte> read(
        std::string_view name) const override;

private:
    [[nodiscard]] std::filesystem::path locate(std::string_view name) const;

    std::filesystem::path root_;
};

}  // namespace konbini::app
