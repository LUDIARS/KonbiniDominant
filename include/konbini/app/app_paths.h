#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

// @implements spec/plan/tasks/first-playable.md Build and process boundary
// @implements spec/interface/pictor-rendering.md Failure

namespace konbini::app {

// runtime が読む外部資産の場所。どちらも起動時に確定し、frame loop 中に
// 変わらない。
struct AppPaths {
    std::filesystem::path contentFile;
    std::filesystem::path shaderDirectory;
};

// world / composite / HUD の 3 pass 分。生成 target (`konbini_shaders`) が
// 出す SPIR-V と 1 対 1 で、欠けていたら起動時に落とす。
inline constexpr std::array<std::string_view, 6> kRequiredShaderFiles{
    "konbini_world.vert.spv",     "konbini_world.frag.spv",
    "konbini_composite.vert.spv", "konbini_composite.frag.spv",
    "konbini_hud.vert.spv",       "konbini_hud.frag.spv",
};

// build 時に埋め込む既定値の上書き。空の値は「未指定」として扱う。
struct AppPathOverrides {
    std::optional<std::string> contentFile;
    std::optional<std::string> shaderDirectory;
};

// 環境変数 `KONBINI_CONTENT_FILE` / `KONBINI_SHADER_DIR` を読む。
[[nodiscard]] AppPathOverrides readAppPathOverridesFromEnvironment();

// 明示された既定パスに環境変数の上書きを適用する。
// 上方探索 fallback は使わない。別 build の古い資産を拾わないため。
[[nodiscard]] AppPaths resolveAppPaths(
    const AppPathOverrides& overrides,
    const std::filesystem::path& defaultContentFile,
    const std::filesystem::path& defaultShaderDirectory);

// 実行ファイル隣の shaders / data/content を既定値に使う版。
// 配布フォルダ全体を移動しても、起動時の cwd に依存しない。
[[nodiscard]] AppPaths resolveAppPaths(const AppPathOverrides& overrides);

// content / shader が揃っているかを検証する。欠落は `std::runtime_error` で、
// placeholder や silent fallback へ落とさない。
void validateAppPaths(const AppPaths& paths);

}  // namespace konbini::app
