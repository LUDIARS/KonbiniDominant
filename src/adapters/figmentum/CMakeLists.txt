set(
  KONBINI_FIGMENTUM_REVISION
  "3ee998f487d984f54003c4ec3c4f7ba00b53eec3"
)
FetchContent_Declare(
  figmentum
  GIT_REPOSITORY https://github.com/LUDIARS/Figmentum.git
  GIT_TAG "${KONBINI_FIGMENTUM_REVISION}"
  GIT_SHALLOW FALSE
)
FetchContent_GetProperties(figmentum)
if(NOT figmentum_POPULATED)
  FetchContent_Populate(figmentum)
endif()

# Validate the checkout before evaluating dependency-owned CMake. This keeps
# a substituted or dirty source tree from executing during configuration.
konbini_verify_exact_git_source(
  "${figmentum_SOURCE_DIR}"
  "${KONBINI_FIGMENTUM_REVISION}"
)
add_subdirectory(
  "${figmentum_SOURCE_DIR}"
  "${figmentum_BINARY_DIR}"
  EXCLUDE_FROM_ALL
)

add_library(
  konbini_figmentum_adapter
  STATIC
    figmentum_city_adapter.cpp
    figmentum_conversions.h
    figmentum_facility_mesher.h
    figmentum_facility_mesher.cpp
    figmentum_plan_projection.h
    figmentum_plan_projection.cpp
    "${PROJECT_SOURCE_DIR}/include/konbini/adapters/figmentum/figmentum_city_adapter.h"
)
target_include_directories(
  konbini_figmentum_adapter
  PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)
target_link_libraries(
  konbini_figmentum_adapter
  PUBLIC
    konbini_city
  PRIVATE
    figmentum_core
)
target_compile_features(konbini_figmentum_adapter PUBLIC cxx_std_20)
konbini_enable_warnings(konbini_figmentum_adapter)
