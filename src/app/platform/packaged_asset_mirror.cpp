#include "konbini/app/platform/packaged_asset_mirror.h"

#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include "konbini/app/platform/relative_resource_name.h"

// @implements spec/interface/mobile-platform.md Assets and generated geometry

namespace konbini::app {

// @implements spec/interface/mobile-platform.md Assets and generated geometry
void materializePackagedAssets(const IAssetReader& reader,
                               const std::span<const std::string_view> names,
                               const std::filesystem::path& root) {
    if (root.empty() || !root.is_absolute()) {
        throw std::invalid_argument("packaged asset mirror root must be absolute: " +
                                    root.string());
    }
    requireAssets(reader, names);
    for (const std::string_view name : names) {
        validateRelativeResourceName(name);
        const std::vector<std::byte> bytes = reader.read(name);
        const std::filesystem::path target = root / std::filesystem::path(name);
        std::filesystem::path staging = target;
        staging += ".partial";

        std::error_code error;
        std::filesystem::create_directories(target.parent_path(), error);
        if (error) {
            throw std::runtime_error("cannot create asset mirror directory " +
                                     target.parent_path().string() + ": " +
                                     error.message());
        }
        {
            std::ofstream output(staging, std::ios::binary | std::ios::trunc);
            output.write(reinterpret_cast<const char*>(bytes.data()),
                         static_cast<std::streamsize>(bytes.size()));
            output.flush();
            if (!output) {
                throw std::runtime_error("cannot write packaged asset " +
                                         std::string(name));
            }
        }
        std::filesystem::rename(staging, target, error);
        if (error) {
            throw std::runtime_error("cannot place packaged asset " +
                                     std::string(name) + ": " + error.message());
        }
    }
}

}  // namespace konbini::app
