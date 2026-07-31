add_library(
  konbini_render_domain
  STATIC
    bitmap_font.cpp
    facility_picker.cpp
    isometric_camera.cpp
    landing_effect_geometry.cpp
    resident_primitive_geometry.cpp
    resident_visual_geometry.cpp
    speech_bubble_geometry.cpp
    store_placement_animation.cpp
    visia.cpp
    visia_geometry.cpp
    visia_geometry_support.h
    world_palette.cpp
    zoc_overlay_geometry.cpp
    "${PROJECT_SOURCE_DIR}/include/konbini/render/bitmap_font.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/render/facility_picker.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/render/isometric_camera.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/render/resident_visual_geometry.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/render/speech_bubble_geometry.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/render/store_placement_animation.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/render/visia.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/render/visia_geometry.h"
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
