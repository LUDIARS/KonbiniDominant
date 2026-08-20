#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#ifndef NOGDI
#define NOGDI
#endif
#include <vulkan/vulkan.h>

// @implements spec/interface/pictor-rendering.md Failure

namespace konbini::adapters::pictor {

// shader ディレクトリは runtime configuration なので、中身が SPIR-V である
// 保証はない。`vkCreateShaderModule` 自体は検証を行わず、任意バイト列を
// 渡した driver は未定義動作になるため、size / alignment / magic word を
// ここで確認する。欠落・不正はすべて例外で、placeholder へ落とさない。
[[nodiscard]] std::vector<std::uint32_t> readSpirv(
    const std::filesystem::path& path);

// 生成した module の破棄は呼び出し側の責務。
[[nodiscard]] VkShaderModule createShaderModule(
    VkDevice device, const std::vector<std::uint32_t>& words);

}  // namespace konbini::adapters::pictor
