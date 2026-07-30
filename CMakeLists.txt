cmake_minimum_required(VERSION 3.28)

project(KonbiniDominant VERSION 0.1.0 LANGUAGES CXX)

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
