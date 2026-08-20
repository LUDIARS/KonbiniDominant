#include "konbini/app/app_paths.h"

#include <cstdlib>
#include <stdexcept>
#include <string>

// @implements spec/plan/tasks/first-playable.md Build and process boundary

#ifndef KONBINI_DEFAULT_CONTENT_FILE
#error "KONBINI_DEFAULT_CONTENT_FILE must be defined by the build"
#endif
#ifndef KONBINI_DEFAULT_SHADER_DIR
#error "KONBINI_DEFAULT_SHADER_DIR must be defined by the build"
#endif

namespace konbini::app {
namespace {

// MSVC は `std::getenv` を C4996 で拒否するので、platform ごとの安全版へ
// 分岐する。空文字列は「未設定」と同じ扱い (誤って空の変数を export した
// ときに、空パスで起動して意味不明な失敗になるのを避ける)。
[[nodiscard]] std::optional<std::string> environmentValue(const char* name) {
#if defined(_MSC_VER)
    char* buffer = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&buffer, &size, name) != 0 || buffer == nullptr) {
        return std::nullopt;
    }
    std::string value(buffer);
    std::free(buffer);
#else
    const char* const raw = std::getenv(name);
    if (raw == nullptr) {
        return std::nullopt;
    }
    std::string value(raw);
#endif
    if (value.empty()) {
        return std::nullopt;
    }
    return value;
}

}  // namespace

AppPathOverrides readAppPathOverridesFromEnvironment() {
    return {
        .contentFile = environmentValue("KONBINI_CONTENT_FILE"),
        .shaderDirectory = environmentValue("KONBINI_SHADER_DIR"),
    };
}

AppPaths resolveAppPaths(
    const AppPathOverrides& overrides,
    const std::filesystem::path& defaultContentFile,
    const std::filesystem::path& defaultShaderDirectory) {
    AppPaths paths{
        .contentFile = overrides.contentFile.has_value()
                           ? std::filesystem::path(*overrides.contentFile)
                           : defaultContentFile,
        .shaderDirectory =
            overrides.shaderDirectory.has_value()
                ? std::filesystem::path(*overrides.shaderDirectory)
                : defaultShaderDirectory,
    };
    if (paths.contentFile.empty() || paths.shaderDirectory.empty()) {
        throw std::runtime_error(
            "content file and shader directory must both be configured");
    }
    return paths;
}

AppPaths resolveAppPaths(const AppPathOverrides& overrides) {
    return resolveAppPaths(
        overrides, std::filesystem::path(KONBINI_DEFAULT_CONTENT_FILE),
        std::filesystem::path(KONBINI_DEFAULT_SHADER_DIR));
}

// @implements spec/interface/pictor-rendering.md Failure
void validateAppPaths(const AppPaths& paths) {
    if (!std::filesystem::is_regular_file(paths.contentFile)) {
        throw std::runtime_error(
            "content file not found: " + paths.contentFile.string());
    }
    if (!std::filesystem::is_directory(paths.shaderDirectory)) {
        throw std::runtime_error(
            "shader directory not found: " +
            paths.shaderDirectory.string());
    }
    for (const std::string_view name : kRequiredShaderFiles) {
        const std::filesystem::path file =
            paths.shaderDirectory / std::filesystem::path(std::string(name));
        if (!std::filesystem::is_regular_file(file)) {
            throw std::runtime_error(
                "required shader missing: " + file.string());
        }
    }
}

}  // namespace konbini::app
