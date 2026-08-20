#include "konbini/adapters/pictor/spirv_module.h"

#include <cstddef>
#include <fstream>
#include <ios>
#include <stdexcept>
#include <string>

// @implements spec/interface/pictor-rendering.md Failure

namespace konbini::adapters::pictor {

namespace {

constexpr std::streamoff kMaxShaderBytes = 64 * 1024 * 1024;
constexpr std::uint32_t kSpirvMagic = 0x07230203U;

}  // namespace

// @implements spec/interface/pictor-rendering.md Failure
std::vector<std::uint32_t> readSpirv(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        throw std::runtime_error(
            "required SPIR-V shader is missing: " + path.string());
    }
    const std::streamoff byteCount = stream.tellg() - std::streampos(0);
    if (byteCount <= 0 || byteCount > kMaxShaderBytes ||
        byteCount % static_cast<std::streamoff>(
                        sizeof(std::uint32_t)) != 0) {
        throw std::runtime_error(
            "invalid SPIR-V shader size: " + path.string());
    }

    std::vector<std::uint32_t> words(
        static_cast<std::size_t>(byteCount) / sizeof(std::uint32_t));
    stream.seekg(0, std::ios::beg);
    stream.read(
        reinterpret_cast<char*>(words.data()),
        static_cast<std::streamsize>(byteCount));
    if (!stream) {
        throw std::runtime_error(
            "failed to read SPIR-V shader: " + path.string());
    }
    if (words.front() != kSpirvMagic) {
        throw std::runtime_error(
            "file is not SPIR-V (bad magic word): " + path.string());
    }
    return words;
}

// @implements spec/interface/pictor-rendering.md Failure
VkShaderModule createShaderModule(
    const VkDevice device, const std::vector<std::uint32_t>& words) {
    if (device == VK_NULL_HANDLE || words.empty()) {
        throw std::invalid_argument(
            "shader module requires a Vulkan device and SPIR-V");
    }

    const VkShaderModuleCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = words.size() * sizeof(std::uint32_t),
        .pCode = words.data(),
    };
    VkShaderModule module = VK_NULL_HANDLE;
    const VkResult result =
        vkCreateShaderModule(device, &info, nullptr, &module);
    if (result != VK_SUCCESS) {
        throw std::runtime_error(
            "vkCreateShaderModule failed with VkResult " +
            std::to_string(static_cast<int>(result)));
    }
    return module;
}

}  // namespace konbini::adapters::pictor
