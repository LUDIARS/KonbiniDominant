#include <cstddef>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "konbini/app/app_paths.h"
#include "konbini/app/platform/asset_reader.h"
#include "konbini/app/platform/directory_asset_reader.h"
#include "konbini/app/platform/relative_resource_name.h"
#include "konbini/app/platform/required_packaged_assets.h"
#include "konbini/app/platform/writable_paths.h"

#include "../../check.h"

// @implements spec/test/verification-strategy.md 1. Data / ID unit tests
// @implements spec/interface/mobile-platform.md Assets and generated geometry

namespace {

using konbini::app::AssetNotFound;
using konbini::app::DirectoryAssetReader;
using konbini::app::IAssetReader;
using konbini::app::kRequiredShaderFiles;
using konbini::app::requireAssets;
using konbini::app::requiredPackagedAssets;
using konbini::app::validateRelativeResourceName;
using konbini::app::WritableArea;
using konbini::app::WritablePathProvider;
using konbini::app::WritableRoots;
using konbini::app::WritableRootUnavailable;

namespace fs = std::filesystem;

// Stands in for AAssetManager / an iOS bundle: names only, no paths.
class MemoryAssetReader final : public IAssetReader {
public:
    void add(const std::string& name, std::vector<std::byte> bytes) {
        assets_[name] = std::move(bytes);
    }
    [[nodiscard]] bool contains(const std::string_view name) const override {
        validateRelativeResourceName(name);
        return assets_.contains(std::string(name));
    }
    [[nodiscard]] std::vector<std::byte> read(const std::string_view name) const override {
        validateRelativeResourceName(name);
        const auto found = assets_.find(std::string(name));
        if (found == assets_.end()) {
            throw AssetNotFound(std::string(name));
        }
        return found->second;
    }

private:
    std::map<std::string, std::vector<std::byte>> assets_;
};

// Scratch directory under the system temp root, removed on scope exit.
class ScratchDirectory {
public:
    explicit ScratchDirectory(const std::string& name)
        : path_(fs::temp_directory_path() / ("konbini_" + name)) {
        std::error_code error;
        fs::remove_all(path_, error);
        fs::create_directories(path_);
    }
    ~ScratchDirectory() {
        std::error_code error;
        fs::remove_all(path_, error);
    }
    ScratchDirectory(const ScratchDirectory&) = delete;
    ScratchDirectory& operator=(const ScratchDirectory&) = delete;

    [[nodiscard]] const fs::path& path() const noexcept { return path_; }

private:
    fs::path path_;
};

void writeFile(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

void resourceNamesStayRelative() {
    CHECK_NO_THROW(validateRelativeResourceName("shaders/konbini_world.vert.spv"));
    CHECK_NO_THROW(validateRelativeResourceName("save.bin"));
    CHECK_THROWS(std::invalid_argument, validateRelativeResourceName(""));
    CHECK_THROWS(std::invalid_argument, validateRelativeResourceName("/data/content"));
    CHECK_THROWS(std::invalid_argument, validateRelativeResourceName("C:/data"));
    CHECK_THROWS(std::invalid_argument, validateRelativeResourceName("data\\content"));
    CHECK_THROWS(std::invalid_argument, validateRelativeResourceName("data/../secret"));
    CHECK_THROWS(std::invalid_argument, validateRelativeResourceName("./data"));
    CHECK_THROWS(std::invalid_argument, validateRelativeResourceName("data//content"));
    CHECK_THROWS(std::invalid_argument, validateRelativeResourceName("data/"));
}

void requiredAssetsCoverContentAndShaders() {
    const std::vector<std::string_view> names = requiredPackagedAssets();
    CHECK(names.size() == 1 + kRequiredShaderFiles.size());
    CHECK(names.front() == "data/content/first-playable.json");
    for (const std::string_view name : names) {
        CHECK_NO_THROW(validateRelativeResourceName(name));
    }
    CHECK(names.back() == "shaders/konbini_hud.frag.spv");
}

void missingRequiredAssetFailsFast() {
    MemoryAssetReader reader;
    const std::vector<std::string_view> names = requiredPackagedAssets();
    for (const std::string_view name : names) {
        reader.add(std::string(name), {std::byte{0x03}});
    }
    CHECK_NO_THROW(requireAssets(reader, names));

    MemoryAssetReader partial;
    partial.add("data/content/first-playable.json", {std::byte{0x7b}});
    bool namedEveryMissing = false;
    try {
        requireAssets(partial, names);
    } catch (const AssetNotFound& error) {
        const std::string message = error.what();
        namedEveryMissing = message.find("shaders/konbini_world.vert.spv") != std::string::npos &&
                            message.find("shaders/konbini_hud.frag.spv") != std::string::npos;
    }
    CHECK(namedEveryMissing);
}

void directoryReaderReadsByName() {
    const ScratchDirectory root("mob003_assets");
    writeFile(root.path() / "data" / "content" / "first-playable.json", "{}");

    CHECK_THROWS(AssetNotFound, DirectoryAssetReader(root.path() / "missing"));
    const DirectoryAssetReader reader(root.path());
    CHECK(reader.contains("data/content/first-playable.json"));
    CHECK(!reader.contains("shaders/konbini_world.vert.spv"));
    const std::vector<std::byte> bytes = reader.read("data/content/first-playable.json");
    CHECK(bytes.size() == 2);
    CHECK(bytes[0] == std::byte{'{'});
    CHECK_THROWS(AssetNotFound, static_cast<void>(reader.read("shaders/konbini_world.vert.spv")));
    // Callers cannot reach outside the packaged root.
    CHECK_THROWS(std::invalid_argument, static_cast<void>(reader.contains("../outside")));
}

void writableRootsAreInjectedAndChecked() {
    const ScratchDirectory scratch("mob003_writable");
    const fs::path base = fs::absolute(scratch.path());
    WritableRoots roots{base / "save", base / "replay", base / "settings", base / "cache",
                        base / "diagnostic"};
    for (const fs::path& root :
         {roots.save, roots.replay, roots.settings, roots.cache, roots.diagnostic}) {
        fs::create_directories(root);
    }

    const WritablePathProvider provider(roots);
    CHECK(provider.root(WritableArea::Save) == roots.save);
    CHECK(provider.root(WritableArea::Cache) == roots.cache);
    CHECK(provider.resolve(WritableArea::Replay, "match/0001.replay") ==
          roots.replay / "match" / "0001.replay");
    CHECK_THROWS(std::invalid_argument,
                 static_cast<void>(provider.resolve(WritableArea::Save, "../escape")));
    CHECK(WritablePathProvider::isRegenerable(WritableArea::Cache));
    CHECK(!WritablePathProvider::isRegenerable(WritableArea::Save));
    CHECK(!WritablePathProvider::isRegenerable(WritableArea::Replay));

    WritableRoots missingCache = roots;
    missingCache.cache.clear();
    CHECK_THROWS(WritableRootUnavailable, WritablePathProvider{missingCache});

    WritableRoots relative = roots;
    relative.settings = "settings";
    CHECK_THROWS(WritableRootUnavailable, WritablePathProvider{relative});

    WritableRoots absent = roots;
    absent.save = base / "not-created";
    CHECK_THROWS(WritableRootUnavailable, WritablePathProvider{absent});
}

}  // namespace

int main() {
    resourceNamesStayRelative();
    requiredAssetsCoverContentAndShaders();
    missingRequiredAssetFailsFast();
    directoryReaderReadsByName();
    writableRootsAreInjectedAndChecked();
    return konbini::test::summarize("konbini_asset_boundary_tests");
}
