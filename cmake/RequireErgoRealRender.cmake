# Consumer-side check of the pinned Ergo render backend contract.
#
# Ergo resolves the real render path from Pictor's PICTOR_HAS_VULKAN contract
# (Ergo cmake/ErgoRenderBackend.cmake) and publishes the result on the
# ergo_render target as ERGO_RENDER_HAS_VULKAN=1 plus one
# ERGO_RENDER_PLATFORM_<platform>=1 definition. KonbiniDominant has no
# render-less game mode, so a configure that produced a Vulkan-free or
# wrong-platform ergo_render must stop here instead of building a headless
# executable that "succeeds".
#
# @implements spec/interface/ergo-runtime.md Render readiness
# @implements spec/interface/mobile-platform.md Surface and renderer

# The Ergo platform definition this configure must select.
function(konbini_expected_ergo_render_platform out_var)
  if(ANDROID)
    set(${out_var} "ANDROID" PARENT_SCOPE)
  elseif(IOS OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(${out_var} "IOS" PARENT_SCOPE)
  else()
    set(${out_var} "DESKTOP" PARENT_SCOPE)
  endif()
endfunction()

# Pure evaluation of an ergo_render INTERFACE_COMPILE_DEFINITIONS list.
# Sets out_var to an empty string when the contract holds, otherwise to the
# reason. Kept free of target access so tests can drive it in script mode.
function(konbini_ergo_render_contract_violation out_var definitions expected_platform)
  set(has_vulkan OFF)
  set(platforms "")
  foreach(definition IN LISTS definitions)
    if(definition STREQUAL "ERGO_RENDER_HAS_VULKAN=1" OR
       definition STREQUAL "ERGO_RENDER_HAS_VULKAN")
      set(has_vulkan ON)
    elseif(definition MATCHES "^ERGO_RENDER_PLATFORM_([A-Z]+)(=1)?$")
      list(APPEND platforms "${CMAKE_MATCH_1}")
    endif()
  endforeach()

  if(NOT has_vulkan)
    set(${out_var} "ergo_render was built without ERGO_RENDER_HAS_VULKAN" PARENT_SCOPE)
    return()
  endif()
  list(LENGTH platforms platform_count)
  if(NOT platform_count EQUAL 1)
    set(${out_var}
      "ergo_render must select exactly one platform, got '${platforms}'"
      PARENT_SCOPE)
    return()
  endif()
  if(NOT platforms STREQUAL expected_platform)
    set(${out_var}
      "ergo_render selected platform ${platforms}, expected ${expected_platform}"
      PARENT_SCOPE)
    return()
  endif()
  set(${out_var} "" PARENT_SCOPE)
endfunction()

function(konbini_require_ergo_real_render)
  if(NOT TARGET ergo_render)
    message(FATAL_ERROR "KonbiniDominant requires the pinned ergo_render target")
  endif()
  get_target_property(definitions ergo_render INTERFACE_COMPILE_DEFINITIONS)
  if(NOT definitions)
    set(definitions "")
  endif()
  konbini_expected_ergo_render_platform(expected_platform)
  konbini_ergo_render_contract_violation(violation "${definitions}" "${expected_platform}")
  if(NOT violation STREQUAL "")
    message(FATAL_ERROR
      "Ergo real render path unavailable: ${violation}. "
      "KonbiniDominant does not fall back to a Vulkan-free build.")
  endif()
  message(STATUS "KonbiniDominant: Ergo real render path selected (platform=${expected_platform})")
endfunction()
