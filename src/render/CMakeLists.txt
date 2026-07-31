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

if(KONBINI_BUILD_RENDER)
  set(konbini_composite_shader_output_directory
      "${CMAKE_BINARY_DIR}/shaders")
  set(konbini_composite_shader_outputs)
  foreach(
    shader_name
    konbini_composite.vert
    konbini_composite.frag
  )
    set(shader_source "${PROJECT_SOURCE_DIR}/shaders/${shader_name}")
    set(
      shader_output
      "${konbini_composite_shader_output_directory}/${shader_name}.spv"
    )
    add_custom_command(
      OUTPUT "${shader_output}"
      COMMAND
        "${CMAKE_COMMAND}" -E make_directory
        "${konbini_composite_shader_output_directory}"
      COMMAND Vulkan::glslc "${shader_source}" -o "${shader_output}"
      DEPENDS "${shader_source}"
      VERBATIM
      COMMENT "Compiling ${shader_name} to SPIR-V"
    )
    list(APPEND konbini_composite_shader_outputs "${shader_output}")
  endforeach()
  add_custom_target(
    konbini_composite_shaders
    DEPENDS ${konbini_composite_shader_outputs}
  )

  add_library(
    konbini_world_composite
    STATIC
      world_composite_layer.cpp
      "${PROJECT_SOURCE_DIR}/include/konbini/render/world_composite_layer.h"
  )

  target_include_directories(
    konbini_world_composite
    PUBLIC
      "${PROJECT_SOURCE_DIR}/include"
  )

  target_compile_features(konbini_world_composite PUBLIC cxx_std_20)
  if(WIN32)
    target_compile_definitions(konbini_world_composite PRIVATE NOGDI)
  endif()
  target_link_libraries(
    konbini_world_composite
    PUBLIC
      ergo_render
      konbini_pictor_scene_targets
    PRIVATE
      pictor
      Vulkan::Vulkan
  )
  add_dependencies(konbini_world_composite konbini_composite_shaders)
  konbini_enable_warnings(konbini_world_composite)
endif()
