cmake_minimum_required(VERSION 3.28)

if(POLICY CMP0169)
  cmake_policy(SET CMP0169 OLD)
endif()

project(KonbiniDominant VERSION 0.1.0 LANGUAGES CXX)

set(KONBINI_MOBILE OFF)
if(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(KONBINI_MOBILE ON)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

include(FetchContent)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyExactGitSource.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/RequireErgoRealRender.cmake")

option(
  KONBINI_BUILD_FIGMENTUM_ADAPTER
  "Build the pinned Figmentum city-generation adapter"
  ON
)
option(
  KONBINI_BUILD_RENDER
  "Build the pinned Pictor/Ergo Vulkan render bridge"
  ON
)
option(KONBINI_BUILD_TESTS "Build KonbiniDominant tests" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  # Match UTF-8 source encoding in game and pinned dependency translation units.
  add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/utf-8>")
  # The portable ZIP carries no developer-machine CRT dependency.
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

function(konbini_enable_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

if(EMSCRIPTEN)
  add_subdirectory(web)
  return()
endif()

if(KONBINI_BUILD_RENDER)
  set(
    KONBINI_PICTOR_REVISION
    "502ba022b66fbba0356b820fc17a064abe37435e"
  )
  set(
    KONBINI_ERGO_REVISION
    "7f0d6bbd34dced4fc6664a5f04bce9910e893537"
  )

  if(KONBINI_MOBILE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/MobileVulkan.cmake")
  else()
    find_package(Vulkan REQUIRED COMPONENTS glslc)
  endif()

  set(PICTOR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(PICTOR_BUILD_DEMO OFF CACHE BOOL "" FORCE)
  set(PICTOR_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    pictor
    GIT_REPOSITORY https://github.com/LUDIARS/Pictor.git
    GIT_TAG "${KONBINI_PICTOR_REVISION}"
    GIT_SHALLOW FALSE
  )
  FetchContent_GetProperties(pictor)
  if(NOT pictor_POPULATED)
    FetchContent_Populate(pictor)
  endif()
  konbini_verify_exact_git_source(
    "${pictor_SOURCE_DIR}"
    "${KONBINI_PICTOR_REVISION}"
  )
  add_subdirectory(
    "${pictor_SOURCE_DIR}"
    "${pictor_BINARY_DIR}"
    EXCLUDE_FROM_ALL
  )

  set(ERGO_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(ERGO_BUILD_DUMMY OFF CACHE BOOL "" FORCE)
  set(ERGO_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
  set(ERGO_BUILD_RENDER ON CACHE BOOL "" FORCE)
  # The game has no Vulkan-free render mode. Make Ergo fail configure on
  # desktop too instead of warning and building a render-less ergo_render
  # (spec/interface/ergo-runtime.md#Render readiness).
  set(ERGO_RENDER_REQUIRE_REAL ON CACHE BOOL "" FORCE)
  # GLFW callback を受ける input adapter と frame clock は KD-FP-000J で
  # 使うので有効にする (spec/interface/ergo-runtime.md#利用候補module)。
  set(ERGO_BUILD_INPUT ON CACHE BOOL "" FORCE)
  set(ERGO_BUILD_FRAME ON CACHE BOOL "" FORCE)
  set(ERGO_BUILD_PARTICLE ON CACHE BOOL "" FORCE)
  set(ERGO_PARTICLE_HAS_RENDERER OFF CACHE BOOL "" FORCE)
  foreach(
    ergo_module
    GPU_PARTICLE
    BIND
    ACTOR
    SOUND
    PROFILE
    LOG
    IO
    AUDIO
    WORLD_TIME
    BLACKBOARD
    UI
    CUSTOS
    HEALTH
    SCORE
    COMBO_COUNTER
    TIMING_JUDGE
    UI_KIT
    UI_LAYOUT
    SHURIKEN_MIGRATOR
    VECTOR
    CAST
    SCENE
    CHARACTER
    MATH
    PHYSICS2D
    RESOURCE
    HTTP
  )
    set("ERGO_BUILD_${ergo_module}" OFF CACHE BOOL "" FORCE)
  endforeach()
  set(ERGO_BUILD_LOG ON CACHE BOOL "" FORCE)
  set(ERGO_PARTICLE_HAS_RENDERER OFF CACHE BOOL "" FORCE)
  set(ERGO_GPU_PARTICLE_COMPILE_SHADERS OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    ergo
    GIT_REPOSITORY https://github.com/LUDIARS/Ergo.git
    GIT_TAG "${KONBINI_ERGO_REVISION}"
    GIT_SHALLOW FALSE
  )
  FetchContent_GetProperties(ergo)
  if(NOT ergo_POPULATED)
    FetchContent_Populate(ergo)
  endif()
  konbini_verify_exact_git_source(
    "${ergo_SOURCE_DIR}"
    "${KONBINI_ERGO_REVISION}"
  )
  add_subdirectory(
    "${ergo_SOURCE_DIR}"
    "${ergo_BINARY_DIR}"
    EXCLUDE_FROM_ALL
  )
  konbini_require_ergo_real_render()
endif()

add_subdirectory(src)
if(KONBINI_MOBILE)
  add_subdirectory(mobile)
endif()

if(KONBINI_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
