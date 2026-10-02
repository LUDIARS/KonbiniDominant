#include "konbini/app/platform/writable_paths.h"

#include <string>
#include <system_error>

#include "konbini/app/platform/relative_resource_name.h"

// @implements spec/interface/mobile-platform.md Assets and generated geometry

namespace konbini::app {
namespace {

[[nodiscard]] std::size_t indexOf(const WritableArea area) noexcept {
    return static_cast<std::size_t>(area);
}

void requireRoot(const WritableArea area, const std::filesystem::path& root) {
    const std::string label(writableAreaName(area));
    if (root.empty()) {
        throw WritableRootUnavailable("writable " + label + " root was not provided");
    }
    if (!root.is_absolute()) {
        throw WritableRootUnavailable(
            "writable " + label + " root must be absolute: " + root.string());
    }
    std::error_code error;
    if (!std::filesystem::is_directory(root, error)) {
        throw WritableRootUnavailable(
            "writable " + label + " root is not a directory: " + root.string());
    }
}

}  // namespace

// @implements spec/interface/mobile-platform.md Assets and generated geometry
// @implements spec/interface/mobile-platform.md Failure policy
WritablePathProvider::WritablePathProvider(const WritableRoots& roots)
    : roots_{roots.save, roots.replay, roots.settings, roots.cache, roots.diagnostic} {
    for (std::size_t index = 0; index < roots_.size(); ++index) {
        requireRoot(static_cast<WritableArea>(index), roots_[index]);
    }
}

const std::filesystem::path& WritablePathProvider::root(
    const WritableArea area) const noexcept {
    return roots_[indexOf(area)];
}

// @implements spec/interface/mobile-platform.md Assets and generated geometry
std::filesystem::path WritablePathProvider::resolve(
    const WritableArea area, const std::string_view name) const {
    validateRelativeResourceName(name);
    return root(area) / std::filesystem::path(std::u8string(name.begin(), name.end()));
}

bool WritablePathProvider::isRegenerable(const WritableArea area) noexcept {
    return area == WritableArea::Cache;
}

std::string_view writableAreaName(const WritableArea area) noexcept {
    switch (area) {
        case WritableArea::Save:
            return "save";
        case WritableArea::Replay:
            return "replay";
        case WritableArea::Settings:
            return "settings";
        case WritableArea::Cache:
            return "cache";
        case WritableArea::Diagnostic:
            return "diagnostic";
    }
    return "unknown";
}

}  // namespace konbini::app
