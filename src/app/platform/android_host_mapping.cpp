#include "konbini/app/platform/android_host_mapping.h"

#include <algorithm>
#include <array>
#include <string>
#include <system_error>
#include <utility>

// @implements spec/interface/mobile-platform.md Required boundaries

namespace konbini::app {
namespace {

[[nodiscard]] std::uint32_t clampedInset(const std::int64_t value,
                                         const std::uint32_t limit) noexcept {
    return static_cast<std::uint32_t>(
        std::clamp<std::int64_t>(value, 0, static_cast<std::int64_t>(limit)));
}

}  // namespace

// @implements spec/interface/mobile-platform.md Lifecycle
std::optional<ThermalLevel> thermalLevelFromAndroidStatus(
    const std::int32_t status) noexcept {
    switch (status) {
        case 0:
            return ThermalLevel::Nominal;
        case 1:
        case 2:
            return ThermalLevel::Fair;
        case 3:
            return ThermalLevel::Serious;
        case 4:
        case 5:
        case 6:
            return ThermalLevel::Critical;
        default:
            return std::nullopt;
    }
}

// @implements spec/interface/mobile-platform.md Required boundaries
SafeAreaInsets safeAreaFromContentRect(const render::ViewportExtent window,
                                       const std::int32_t left,
                                       const std::int32_t top,
                                       const std::int32_t right,
                                       const std::int32_t bottom) noexcept {
    // An empty rect (not reported yet) means the whole window is usable.
    if (right <= left || bottom <= top) {
        return {};
    }
    const auto width = static_cast<std::int64_t>(window.width);
    const auto height = static_cast<std::int64_t>(window.height);
    return {
        .left = clampedInset(left, window.width),
        .top = clampedInset(top, window.height),
        .right = clampedInset(width - right, window.width),
        .bottom = clampedInset(height - bottom, window.height),
    };
}

AndroidAppDirectories androidAppDirectories(
    const std::filesystem::path& internalDataPath) {
    if (internalDataPath.empty() || !internalDataPath.is_absolute()) {
        throw WritableRootUnavailable(
            "Android internal data path is empty or relative: " +
            internalDataPath.string());
    }
    const std::filesystem::path files = internalDataPath.lexically_normal();
    return {
        .files = files,
        .cache = files.parent_path() / "cache",
    };
}

// @implements spec/interface/mobile-platform.md Assets and generated geometry
WritableRoots androidWritableRoots(const AndroidAppDirectories& directories) {
    const std::filesystem::path persistent = directories.files / "konbini";
    return {
        .save = persistent / "save",
        .replay = persistent / "replay",
        .settings = persistent / "settings",
        .cache = directories.cache / "konbini" / "geometry",
        .diagnostic = persistent / "diagnostics",
    };
}

std::filesystem::path androidPackageMirrorRoot(
    const AndroidAppDirectories& directories) {
    return directories.cache / "konbini" / "package";
}

void createWritableRoots(const WritableRoots& roots) {
    const std::array<std::pair<WritableArea, const std::filesystem::path*>,
                     kWritableAreaCount>
        areas{{
            {WritableArea::Save, &roots.save},
            {WritableArea::Replay, &roots.replay},
            {WritableArea::Settings, &roots.settings},
            {WritableArea::Cache, &roots.cache},
            {WritableArea::Diagnostic, &roots.diagnostic},
        }};
    for (const auto& [area, root] : areas) {
        std::error_code error;
        std::filesystem::create_directories(*root, error);
        if (error || !std::filesystem::is_directory(*root)) {
            throw WritableRootUnavailable(
                "cannot create writable " + std::string(writableAreaName(area)) +
                " root " + root->string() +
                (error ? ": " + error.message() : std::string()));
        }
    }
}

}  // namespace konbini::app
