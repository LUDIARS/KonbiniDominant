#pragma once

#include "konbini/app/platform/asset_reader.h"

struct AAssetManager;

namespace konbini::android_host {

// Read-only APK assets through `AAssetManager`. Names are package-relative
// (`shaders/konbini_world.vert.spv`); nothing is assumed to exist on disk.
// @implements spec/interface/mobile-platform.md Assets and generated geometry
class AndroidAssetReader final : public app::IAssetReader {
public:
    // The manager belongs to the activity and outlives the reader.
    explicit AndroidAssetReader(AAssetManager* manager);

    [[nodiscard]] bool contains(std::string_view name) const override;
    [[nodiscard]] std::vector<std::byte> read(std::string_view name) const override;

private:
    AAssetManager* manager_ = nullptr;
};

}  // namespace konbini::android_host
