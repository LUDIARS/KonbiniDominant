# Script-mode test for cmake/RequireErgoRealRender.cmake.
#
# Android / iOS toolchains are not available on every review host, so the
# platform selection is exercised on the pure evaluation function with the
# definition lists Ergo publishes for each platform.
# @implements spec/interface/ergo-runtime.md Render readiness
# @implements spec/interface/mobile-platform.md Surface and renderer

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/RequireErgoRealRender.cmake")

function(expect_contract name definitions platform expected_ok)
  konbini_ergo_render_contract_violation(violation "${definitions}" "${platform}")
  if(expected_ok AND NOT violation STREQUAL "")
    message(SEND_ERROR "FAIL ${name}: expected contract to hold, got: ${violation}")
  elseif(NOT expected_ok AND violation STREQUAL "")
    message(SEND_ERROR "FAIL ${name}: expected a contract violation")
  endif()
endfunction()

set(source "ERGO_RENDER_VULKAN_SOURCE=\"Android NDK\"")

expect_contract(desktop_real
  "ERGO_RENDER_PLATFORM_DESKTOP=1;ERGO_RENDER_HAS_VULKAN=1;${source}" DESKTOP TRUE)
expect_contract(android_real
  "ERGO_RENDER_PLATFORM_ANDROID=1;ERGO_RENDER_HAS_VULKAN=1;${source}" ANDROID TRUE)
expect_contract(ios_real
  "ERGO_RENDER_PLATFORM_IOS=1;ERGO_RENDER_HAS_VULKAN=1;${source}" IOS TRUE)

# Vulkan-free ergo_render (desktop warning path in Ergo) must be rejected.
expect_contract(vulkan_free "" DESKTOP FALSE)
expect_contract(platform_without_vulkan "ERGO_RENDER_PLATFORM_ANDROID=1" ANDROID FALSE)
# Mobile must not silently resolve to the desktop contract.
expect_contract(android_got_desktop
  "ERGO_RENDER_PLATFORM_DESKTOP=1;ERGO_RENDER_HAS_VULKAN=1" ANDROID FALSE)
expect_contract(ios_got_android
  "ERGO_RENDER_PLATFORM_ANDROID=1;ERGO_RENDER_HAS_VULKAN=1" IOS FALSE)
expect_contract(two_platforms
  "ERGO_RENDER_PLATFORM_DESKTOP=1;ERGO_RENDER_PLATFORM_IOS=1;ERGO_RENDER_HAS_VULKAN=1"
  IOS FALSE)

# Platform selection follows the configure toolchain variables.
set(ANDROID ON)
konbini_expected_ergo_render_platform(platform)
if(NOT platform STREQUAL "ANDROID")
  message(SEND_ERROR "FAIL android toolchain selected ${platform}")
endif()
unset(ANDROID)
set(CMAKE_SYSTEM_NAME "iOS")
konbini_expected_ergo_render_platform(platform)
if(NOT platform STREQUAL "IOS")
  message(SEND_ERROR "FAIL iOS toolchain selected ${platform}")
endif()
set(CMAKE_SYSTEM_NAME "Windows")
konbini_expected_ergo_render_platform(platform)
if(NOT platform STREQUAL "DESKTOP")
  message(SEND_ERROR "FAIL desktop toolchain selected ${platform}")
endif()

message(STATUS "ergo real render contract checks finished")
