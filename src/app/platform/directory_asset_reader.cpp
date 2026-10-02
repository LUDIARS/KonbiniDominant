#include "konbini/app/platform/directory_asset_reader.h"

#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>

#include "konbini/app/platform/relative_resource_name.h"

// @implements spec/interface/mobile-platform.md Assets and generated geometry

namespace konbini::app {

DirectoryAssetReader::DirectoryAssetReader(std::filesystem::path root)
    : root_(std::move(root)) {
    std::error_code error;
    if (root_.empty() || !std::filesystem::is_directory(root_, error)) {
        throw AssetNotFound("packaged asset root is not a directory: " + root_.string());
    }
}

std::filesystem::path DirectoryAssetReader::locate(const std::string_view name) const {
    validateRelativeResourceName(name);
    return root_ / std::filesystem::path(std::u8string(name.begin(), name.end()));
}

// @implements spec/interface/mobile-platform.md Assets and generated geometry
bool DirectoryAssetReader::contains(const std::string_view name) const {
    std::error_code error;
    return std::filesystem::is_regular_file(locate(name), error);
}

// @implements spec/interface/mobile-platform.md Assets and generated geometry
std::vector<std::byte> DirectoryAssetReader::read(const std::string_view name) const {
    const std::filesystem::path path = locate(name);
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw AssetNotFound("packaged asset missing: " + std::string(name));
    }
    const std::vector<char> raw(
        (std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (stream.bad()) {
        throw AssetNotFound("packaged asset unreadable: " + std::string(name));
    }
    std::vector<std::byte> bytes(raw.size());
    for (std::size_t index = 0; index < raw.size(); ++index) {
        bytes[index] = static_cast<std::byte>(raw[index]);
    }
    return bytes;
}

}  // namespace konbini::app
