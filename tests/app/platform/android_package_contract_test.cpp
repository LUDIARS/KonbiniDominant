#include <cstddef>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "konbini/app/platform/android_host_mapping.h"
#include "konbini/app/platform/asset_reader.h"
#include "konbini/app/platform/graphics_profile_diagnostic.h"
#include "konbini/app/platform/mobile_graphics_profile.h"
#include "konbini/app/platform/packaged_asset_mirror.h"
#include "konbini/app/platform/relative_resource_name.h"
#include "konbini/app/platform/required_packaged_assets.h"
#include "konbini/app/platform/writable_paths.h"

#include "../../check.h"

// @implements spec/test/verification-strategy.md 1. Data / ID unit tests
// @implements spec/interface/mobile-platform.md Surface and renderer
// @implements spec/interface/mobile-platform.md Assets and generated geometry

namespace {

namespace app = konbini::app;
namespace city = konbini::city;
namespace fs = std::filesystem;
using app::MobileGraphicsProfileId;
using app::ThermalLevel;

class MemoryAssetReader final : public app::IAssetReader {
public:
    void add(const std::string& name, const std::string& text) {
        std::vector<std::byte> bytes;
        for (const char value : text) {
            bytes.push_back(static_cast<std::byte>(value));
        }
        assets_[name] = std::move(bytes);
    }
    [[nodiscard]] bool contains(const std::string_view name) const override {
        app::validateRelativeResourceName(name);
        return assets_.contains(std::string(name));
    }
    [[nodiscard]] std::vector<std::byte> read(const std::string_view name) const override {
        app::validateRelativeResourceName(name);
        const auto found = assets_.find(std::string(name));
        if (found == assets_.end()) {
            throw app::AssetNotFound(std::string(name));
        }
        return found->second;
    }

private:
    std::map<std::string, std::vector<std::byte>> assets_;
};

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        std::random_device seed;
        path_ = fs::temp_directory_path() / ("konbini-android-package-" + std::to_string(seed()));
        fs::create_directories(path_);
    }
    ~TemporaryDirectory() {
        std::error_code ignored;
        fs::remove_all(path_, ignored);
    }
    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
    [[nodiscard]] const fs::path& path() const noexcept { return path_; }

private:
    fs::path path_;
};

[[nodiscard]] std::string readText(const fs::path& file) {
    std::ifstream input(file, std::ios::binary);
    std::ostringstream text;
    text << input.rdbuf();
    return text.str();
}

void testProfileSelectionIsExplicit() {
    CHECK(app::selectMobileGraphicsProfile(std::nullopt).profile == MobileGraphicsProfileId::MobileHigh);
    CHECK(app::selectMobileGraphicsProfile(ThermalLevel::Nominal).profile == MobileGraphicsProfileId::MobileHigh);
    CHECK(app::selectMobileGraphicsProfile(ThermalLevel::Fair).profile == MobileGraphicsProfileId::MobileHigh);
    CHECK(app::selectMobileGraphicsProfile(ThermalLevel::Serious).profile == MobileGraphicsProfileId::MobileLow);
    CHECK(app::selectMobileGraphicsProfile(ThermalLevel::Critical).profile == MobileGraphicsProfileId::MobileLow);
    CHECK(!app::selectMobileGraphicsProfile(std::nullopt).thermal.has_value());
}

// Every mobile profile is coarser than desktop and keeps the world target
// formats WorldSceneTargets capability-checks; no profile is a silent
// format fallback.
void testProfilesDeclareLowLodAndCheckedFormats() {
    const auto& high = app::mobileGraphicsProfile(MobileGraphicsProfileId::MobileHigh);
    const auto& low = app::mobileGraphicsProfile(MobileGraphicsProfileId::MobileLow);
    CHECK(high.name == "MobileHigh");
    CHECK(low.name == "MobileLow");
    for (const auto* profile : {&high, &low}) {
        CHECK(profile->version == app::kMobileGraphicsProfileVersion);
        CHECK(profile->worldColorFormat == "R16G16B16A16_SFLOAT");
        CHECK(profile->worldDepthFormat == "D32_SFLOAT");
        CHECK(profile->renderScale == 1.0);
        CHECK(profile->facilityMesh.polygonizeResolution > 0);
        CHECK(profile->facilityMesh.polygonizeResolution < city::kFirstPlayableFacilityPolygonizeResolution);
        CHECK(profile->facilityMesh.lod > city::kFirstPlayableFacilityLod);
    }
    CHECK(low.facilityMesh.polygonizeResolution < high.facilityMesh.polygonizeResolution);
    CHECK(low.facilityMesh.lod > high.facilityMesh.lod);
}

void testSelectionDescriptionNamesEveryInput() {
    const std::string unknown = app::describeMobileGraphicsProfileSelection(app::selectMobileGraphicsProfile(std::nullopt));
    CHECK(unknown.find("graphics-profile=MobileHigh") != std::string::npos);
    CHECK(unknown.find("thermal=unavailable") != std::string::npos);
    CHECK(unknown.find("facility-lod=1") != std::string::npos);
    const std::string hot = app::describeMobileGraphicsProfileSelection(app::selectMobileGraphicsProfile(ThermalLevel::Serious));
    CHECK(hot.find("graphics-profile=MobileLow") != std::string::npos);
    CHECK(hot.find("thermal=serious") != std::string::npos);
}

void testAndroidThermalStatusMapping() {
    CHECK(app::thermalLevelFromAndroidStatus(0) == ThermalLevel::Nominal);
    CHECK(app::thermalLevelFromAndroidStatus(1) == ThermalLevel::Fair);
    CHECK(app::thermalLevelFromAndroidStatus(2) == ThermalLevel::Fair);
    CHECK(app::thermalLevelFromAndroidStatus(3) == ThermalLevel::Serious);
    CHECK(app::thermalLevelFromAndroidStatus(4) == ThermalLevel::Critical);
    CHECK(app::thermalLevelFromAndroidStatus(6) == ThermalLevel::Critical);
    CHECK(!app::thermalLevelFromAndroidStatus(-1).has_value());
    CHECK(!app::thermalLevelFromAndroidStatus(7).has_value());
}

void testContentRectBecomesSafeArea() {
    const konbini::render::ViewportExtent window{2400, 1080};
    const app::SafeAreaInsets cutout = app::safeAreaFromContentRect(window, 96, 0, 2400, 1032);
    CHECK(cutout.left == 96);
    CHECK(cutout.top == 0);
    CHECK(cutout.right == 0);
    CHECK(cutout.bottom == 48);
    // Not reported yet: the whole window is usable.
    CHECK(app::safeAreaFromContentRect(window, 0, 0, 0, 0) == app::SafeAreaInsets{});
    // A stale rect larger than the window never yields negative insets.
    const app::SafeAreaInsets stale = app::safeAreaFromContentRect(window, -10, -10, 2600, 1200);
    CHECK(stale == app::SafeAreaInsets{});
}

void testWritableRootsAreSeparatedFromThePackage() {
    CHECK_THROWS(app::WritableRootUnavailable, app::androidAppDirectories(fs::path()));
    CHECK_THROWS(app::WritableRootUnavailable, app::androidAppDirectories(fs::path("files")));

    const TemporaryDirectory sandbox;
    const fs::path files = sandbox.path() / "com.ludiars.konbinidominant" / "files";
    const app::AndroidAppDirectories directories = app::androidAppDirectories(files);
    CHECK(directories.files == files.lexically_normal());
    CHECK(directories.cache == files.lexically_normal().parent_path() / "cache");

    const app::WritableRoots roots = app::androidWritableRoots(directories);
    CHECK(roots.save == directories.files / "konbini" / "save");
    CHECK(roots.diagnostic == directories.files / "konbini" / "diagnostics");
    CHECK(roots.cache == directories.cache / "konbini" / "geometry");
    const fs::path mirror = app::androidPackageMirrorRoot(directories);
    CHECK(mirror == directories.cache / "konbini" / "package");
    CHECK(mirror != roots.cache);

    app::createWritableRoots(roots);
    const app::WritablePathProvider provider(roots);
    CHECK(provider.root(app::WritableArea::Settings) == roots.settings);
    CHECK(fs::is_directory(roots.replay));
}

void testDiagnosticRecordsSelectionAndHeldThermal() {
    const TemporaryDirectory sandbox;
    const app::WritableRoots roots = app::androidWritableRoots(app::androidAppDirectories(sandbox.path() / "files"));
    app::createWritableRoots(roots);
    const app::WritablePathProvider provider(roots);

    const app::GraphicsProfileDiagnostic diagnostic(provider, app::selectMobileGraphicsProfile(ThermalLevel::Fair));
    CHECK(diagnostic.file() == roots.diagnostic / std::string(app::kGraphicsProfileDiagnosticName));
    diagnostic.recordThermalChange(ThermalLevel::Critical);
    const std::string text = readText(diagnostic.file());
    CHECK(text.find("graphics-profile=MobileHigh") != std::string::npos);
    CHECK(text.find("thermal=fair") != std::string::npos);
    CHECK(text.find("thermal=critical graphics-profile=MobileHigh held") != std::string::npos);

    // A new boot starts a new record.
    const app::GraphicsProfileDiagnostic next(provider, app::selectMobileGraphicsProfile(ThermalLevel::Serious));
    CHECK(readText(next.file()).find("held") == std::string::npos);
}

void testMirrorMaterializesEveryRequiredAsset() {
    MemoryAssetReader reader;
    for (const std::string_view name : app::requiredPackagedAssets()) {
        reader.add(std::string(name), "bytes:" + std::string(name));
    }
    const TemporaryDirectory sandbox;
    const fs::path root = sandbox.path() / "package";
    const auto names = app::requiredPackagedAssets();
    app::materializePackagedAssets(reader, names, root);
    for (const std::string_view name : names) {
        CHECK(readText(root / fs::path(name)) == "bytes:" + std::string(name));
        fs::path staging = root / fs::path(name);
        staging += ".partial";
        CHECK(!fs::exists(staging));
    }
    // Re-materializing replaces the previous boot's files.
    app::materializePackagedAssets(reader, names, root);
    CHECK(readText(root / "data/content/first-playable.json") == "bytes:data/content/first-playable.json");
}

void testMirrorFailsBeforeWritingWhenAnAssetIsMissing() {
    MemoryAssetReader reader;
    reader.add("data/content/first-playable.json", "{}");
    const TemporaryDirectory sandbox;
    const fs::path root = sandbox.path() / "package";
    const auto names = app::requiredPackagedAssets();
    std::string message;
    try {
        app::materializePackagedAssets(reader, names, root);
    } catch (const app::AssetNotFound& error) {
        message = error.what();
    }
    CHECK(message.find("shaders/konbini_world.vert.spv") != std::string::npos);
    CHECK(message.find("shaders/konbini_hud.frag.spv") != std::string::npos);
    CHECK(!fs::exists(root));
    CHECK_THROWS(std::invalid_argument, app::materializePackagedAssets(reader, names, fs::path("package")));
}

// mobile/android/required-assets.txt drives the Gradle package checks; it must
// name exactly what the runtime refuses to start without.
void testGradleRequiredAssetListMatchesTheRuntime() {
    std::ifstream input(KONBINI_ANDROID_REQUIRED_ASSETS_FILE);
    CHECK(static_cast<bool>(input));
    std::vector<std::string> listed;
    for (std::string line; std::getline(input, line);) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
            line.pop_back();
        }
        if (!line.empty() && line.front() != '#') {
            listed.push_back(line);
        }
    }
    const auto runtime = app::requiredPackagedAssets();
    CHECK(listed.size() == runtime.size());
    for (std::size_t index = 0; index < runtime.size() && index < listed.size(); ++index) {
        CHECK(listed[index] == runtime[index]);
    }
}

}  // namespace

int main() {
    testProfileSelectionIsExplicit();
    testProfilesDeclareLowLodAndCheckedFormats();
    testSelectionDescriptionNamesEveryInput();
    testAndroidThermalStatusMapping();
    testContentRectBecomesSafeArea();
    testWritableRootsAreSeparatedFromThePackage();
    testDiagnosticRecordsSelectionAndHeldThermal();
    testMirrorMaterializesEveryRequiredAsset();
    testMirrorFailsBeforeWritingWhenAnAssetIsMissing();
    testGradleRequiredAssetListMatchesTheRuntime();
    return konbini::test::summarize("konbini_android_package_contract_tests");
}
