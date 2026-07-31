cmake_minimum_required(VERSION 3.28)

if(POLICY CMP0169)
  cmake_policy(SET CMP0169 OLD)
endif()

project(KonbiniDominant VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyExactGitSource.cmake")

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

function(konbini_enable_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

if(KONBINI_BUILD_RENDER)
  set(
    KONBINI_PICTOR_REVISION
    "c088e8d1b7b9e2625b7a8d923c89d4d684566c16"
  )
  set(
    KONBINI_ERGO_REVISION
    "771b027f0e5492015b27f54c3bab1fd5c1ae4790"
  )

  find_package(Vulkan REQUIRED COMPONENTS glslc)

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
  foreach(
    ergo_module
    INPUT
    PARTICLE
    GPU_PARTICLE
    BIND
    ACTOR
    SOUND
    FRAME
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
endif()

add_subdirectory(src)

if(KONBINI_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
