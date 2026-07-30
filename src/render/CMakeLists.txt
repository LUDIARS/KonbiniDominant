add_library(
  konbini_render_domain
  STATIC
    facility_picker.cpp
    isometric_camera.cpp
    world_palette.cpp
    zoc_overlay_geometry.cpp
    "${PROJECT_SOURCE_DIR}/include/konbini/render/facility_picker.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/render/isometric_camera.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/render/viewport_extent.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/render/world_palette.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/render/world_ray.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/render/world_vertex.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/render/zoc_overlay_geometry.h"
)

target_include_directories(
  konbini_render_domain
  PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

target_compile_features(konbini_render_domain PUBLIC cxx_std_20)
target_link_libraries(konbini_render_domain PUBLIC konbini_sim)
konbini_enable_warnings(konbini_render_domain)
