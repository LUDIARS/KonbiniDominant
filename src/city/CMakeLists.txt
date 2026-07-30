add_library(
  konbini_city
  STATIC
    canonical_bytes.cpp
    canonical_bytes.h
    city_manifest.cpp
    city_manifest_canonical.cpp
    city_manifest_projection.cpp
    facility_geometry_cache.cpp
    "${PROJECT_SOURCE_DIR}/include/konbini/city/city_manifest.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/city/city_manifest_canonical.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/city/city_manifest_projection.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/city/facility_geometry.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/city/facility_geometry_cache.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/city/i_city_generator.h"
)

target_include_directories(
  konbini_city
  PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

target_compile_features(konbini_city PUBLIC cxx_std_20)
target_link_libraries(konbini_city PUBLIC konbini_sim)
konbini_enable_warnings(konbini_city)
