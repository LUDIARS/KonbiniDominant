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

add_subdirectory(src)

if(KONBINI_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
