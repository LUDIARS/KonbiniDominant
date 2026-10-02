#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string_view>

// @implements spec/interface/mobile-platform.md Assets and generated geometry

namespace konbini::app {

enum class WritableArea : std::uint8_t {
    Save,
    Replay,
    Settings,
    Cache,       // regenerable derived data (geometry cache)
    Diagnostic,
};

inline constexpr std::size_t kWritableAreaCount = 5;

class WritableRootUnavailable : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Roots the platform host resolved (Android `getFilesDir` / `getCacheDir`,
// iOS Application Support / Caches, desktop user data directory).
struct WritableRoots {
    std::filesystem::path save;
    std::filesystem::path replay;
    std::filesystem::path settings;
    std::filesystem::path cache;
    std::filesystem::path diagnostic;
};

// Injected writable locations. The game never guesses a writable directory
// from the executable or the working directory.
// @implements spec/interface/mobile-platform.md Assets and generated geometry
class WritablePathProvider {
public:
    // Every root must be absolute and an existing directory; otherwise
    // throws `WritableRootUnavailable` naming the area (fail-fast boot).
    explicit WritablePathProvider(const WritableRoots& roots);

    [[nodiscard]] const std::filesystem::path& root(
        WritableArea area) const noexcept;
    // `name` is validated by `validateRelativeResourceName`.
    [[nodiscard]] std::filesystem::path resolve(
        WritableArea area, std::string_view name) const;

    // Only the cache area may be wiped under memory pressure.
    [[nodiscard]] static bool isRegenerable(WritableArea area) noexcept;

private:
    std::array<std::filesystem::path, kWritableAreaCount> roots_;
};

[[nodiscard]] std::string_view writableAreaName(WritableArea area) noexcept;

}  // namespace konbini::app
