#include "android_asset_reader.h"

#include <android/asset_manager.h>

#include <memory>
#include <stdexcept>
#include <string>

#include "konbini/app/platform/relative_resource_name.h"

namespace konbini::android_host {
namespace {

using AssetHandle = std::unique_ptr<AAsset, decltype(&AAsset_close)>;

[[nodiscard]] AssetHandle open(AAssetManager* manager, const std::string_view name) {
    app::validateRelativeResourceName(name);
    const std::string terminated(name);
    return {AAssetManager_open(manager, terminated.c_str(), AASSET_MODE_STREAMING), AAsset_close};
}

}  // namespace

AndroidAssetReader::AndroidAssetReader(AAssetManager* manager) : manager_(manager) {
    if (manager_ == nullptr) {
        throw std::invalid_argument("Android asset manager is unavailable");
    }
}

bool AndroidAssetReader::contains(const std::string_view name) const {
    return open(manager_, name) != nullptr;
}

std::vector<std::byte> AndroidAssetReader::read(const std::string_view name) const {
    const AssetHandle asset = open(manager_, name);
    if (!asset) {
        throw app::AssetNotFound("missing packaged asset: " + std::string(name));
    }
    std::vector<std::byte> bytes;
    const off64_t length = AAsset_getLength64(asset.get());
    if (length > 0) {
        bytes.reserve(static_cast<std::size_t>(length));
    }
    std::byte buffer[16384];
    int count = 0;
    while ((count = AAsset_read(asset.get(), buffer, sizeof(buffer))) > 0) {
        bytes.insert(bytes.end(), buffer, buffer + count);
    }
    if (count < 0) {
        throw std::runtime_error("cannot read packaged asset: " + std::string(name));
    }
    return bytes;
}

}  // namespace konbini::android_host
