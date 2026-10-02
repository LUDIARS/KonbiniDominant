#pragma once

#include <cstdint>

#ifndef NOGDI
#define NOGDI
#endif
#include <vulkan/vulkan.h>

// @implements spec/interface/pictor-rendering.md Required bridge

namespace konbini::adapters::pictor {

// CPU から毎フレーム書ける HOST_VISIBLE | HOST_COHERENT のみを対象にする。
// COHERENT を必須にしているのは、非 coherent メモリの明示 flush を省くため
// ではなく、flush 範囲を nonCoherentAtomSize へ丸める責務を各 owner へ
// 持ち込まないため。該当が無い device は fallback せず `std::runtime_error`。
[[nodiscard]] std::uint32_t findHostVisibleCoherentMemoryType(
    VkPhysicalDevice physicalDevice, std::uint32_t typeBits);

}  // namespace konbini::adapters::pictor
