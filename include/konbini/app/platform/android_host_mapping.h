#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>

#include "konbini/app/platform/display_metrics.h"
#include "konbini/app/platform/memory_pressure.h"
#include "konbini/app/platform/writable_paths.h"
#include "konbini/render/viewport_extent.h"

// @implements spec/interface/mobile-platform.md Required boundaries

// Normalizes Android host values into the shared platform boundary. It takes
// the NDK's numeric values, not NDK types, so the mapping builds and is
// tested on every host; only mobile/android includes the NDK headers.
namespace konbini::app {

// `AThermalStatus` (android/thermal.h): NONE 0, LIGHT 1, MODERATE 2,
// SEVERE 3, CRITICAL 4, EMERGENCY 5, SHUTDOWN 6, ERROR -1. ERROR and any
// value outside the enum mean "no thermal status" and return empty.
// @implements spec/interface/mobile-platform.md Lifecycle
[[nodiscard]] std::optional<ThermalLevel> thermalLevelFromAndroidStatus(
    std::int32_t status) noexcept;

// `android_app::contentRect` is the part of the window the system bars and
// cutouts leave to the app. The insets are the window edges outside it,
// clamped to the window so a stale rect cannot produce negative insets.
// @implements spec/interface/mobile-platform.md Required boundaries
[[nodiscard]] SafeAreaInsets safeAreaFromContentRect(
    render::ViewportExtent window, std::int32_t left, std::int32_t top,
    std::int32_t right, std::int32_t bottom) noexcept;

// Private app directories derived from `ANativeActivity::internalDataPath`
// (`.../<package>/files`). `cache` is its sibling `.../<package>/cache`, the
// directory `Context.getCacheDir()` returns; the OS may clear it.
struct AndroidAppDirectories {
    std::filesystem::path files;
    std::filesystem::path cache;
};

// Throws `WritableRootUnavailable` for an empty or relative path.
[[nodiscard]] AndroidAppDirectories androidAppDirectories(
    const std::filesystem::path& internalDataPath);

// save / replay / settings / diagnostics under `files/konbini`, the
// regenerable geometry cache under `cache/konbini/geometry`.
// @implements spec/interface/mobile-platform.md Assets and generated geometry
[[nodiscard]] WritableRoots androidWritableRoots(
    const AndroidAppDirectories& directories);

// Where packaged assets are materialized for loaders that take a path. It is
// cache: the APK stays the read-only source and the mirror is rebuilt on
// every boot.
[[nodiscard]] std::filesystem::path androidPackageMirrorRoot(
    const AndroidAppDirectories& directories);

// Creates every root of `roots`; a failure throws `WritableRootUnavailable`
// naming the area, so the boot stops before the game writes anything.
void createWritableRoots(const WritableRoots& roots);

}  // namespace konbini::app
