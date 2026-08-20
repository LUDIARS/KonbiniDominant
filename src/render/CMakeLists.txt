add_library(
  konbini_render_domain
  STATIC
    bitmap_font.cpp
    facility_picker.cpp
    facility_world_mesh.cpp
    hud_text_geometry.cpp
    isometric_camera.cpp
    landing_effect_geometry.cpp
    resident_primitive_geometry.cpp
    resident_visual_geometry.cpp
    selection_overlay_geometry.cpp
    speech_bubble_geometry.cpp
    store_marker_geometry.cpp
    store_placement_animation.cpp
    visia.cpp
    visia_geometry.cpp
    visia_geometry_support.h
    world_box_geometry.h
    world_draw_list.cpp
    world_palette.cpp
    zoc_overlay_geometry.cpp
    "${PROJECT_SOURCE_DIR}/include/konbini/render/bitmap_font.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/render/facility_picker.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/render/facility_world_mesh.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/render/hud_text_geometry.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/render/isometric_camera.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/render/resident_visual_geometry.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/render/selection_overlay_geometry.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/render/speech_bubble_geometry.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/render/store_marker_geometry.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/render/store_placement_animation.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/render/visia.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/render/visia_geometry.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/render/viewport_extent.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/render/world_draw_list.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/render/world_mesh.h"
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
# Figmentum 由来の facility geometry を `WorldVertex` へ射影するため city へ
# 依存する。city は sim だけに依存するので循環はしない。
target_link_libraries(konbini_render_domain PUBLIC konbini_sim konbini_city)
konbini_enable_warnings(konbini_render_domain)

if(KONBINI_BUILD_RENDER)
  set(konbini_shader_output_directory "${CMAKE_BINARY_DIR}/shaders")
  set(konbini_shader_outputs)
  foreach(
    shader_name
    konbini_composite.vert
    konbini_composite.frag
    konbini_world.vert
    konbini_world.frag
    konbini_hud.vert
    konbini_hud.frag
  )
    set(shader_source "${PROJECT_SOURCE_DIR}/shaders/${shader_name}")
    set(
      shader_output
      "${konbini_shader_output_directory}/${shader_name}.spv"
    )
    add_custom_command(
      OUTPUT "${shader_output}"
      COMMAND
        "${CMAKE_COMMAND}" -E make_directory
        "${konbini_shader_output_directory}"
      COMMAND Vulkan::glslc "${shader_source}" -o "${shader_output}"
      DEPENDS "${shader_source}"
      VERBATIM
      COMMENT "Compiling ${shader_name} to SPIR-V"
    )
    list(APPEND konbini_shader_outputs "${shader_output}")
  endforeach()
  add_custom_target(
    konbini_shaders
    DEPENDS ${konbini_shader_outputs}
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
      konbini_pictor_world_geometry
      pictor
      Vulkan::Vulkan
  )
  add_dependencies(konbini_world_composite konbini_shaders)
  konbini_enable_warnings(konbini_world_composite)

  add_library(
    konbini_world_render
    STATIC
      world_pipelines.cpp
      world_pipelines.h
      world_render_layer.cpp
      "${PROJECT_SOURCE_DIR}/include/konbini/render/world_render_layer.h"
  )

  target_include_directories(
    konbini_world_render
    PUBLIC
      "${PROJECT_SOURCE_DIR}/include"
  )

  target_compile_features(konbini_world_render PUBLIC cxx_std_20)
  if(WIN32)
    target_compile_definitions(konbini_world_render PRIVATE NOGDI)
  endif()
  target_link_libraries(
    konbini_world_render
    PUBLIC
      ergo_render
      konbini_pictor_scene_targets
      konbini_pictor_world_geometry
      konbini_render_domain
    PRIVATE
      pictor
      Vulkan::Vulkan
  )
  add_dependencies(konbini_world_render konbini_shaders)
  konbini_enable_warnings(konbini_world_render)

  # HUD は swapchain pass (pass 1) 側の layer。world pass の pipeline とは
  # depth / blend 構成が異なるので、別ライブラリとして責務を分ける。
  add_library(
    konbini_hud_overlay
    STATIC
      hud_overlay_layer.cpp
      hud_pipelines.cpp
      hud_pipelines.h
      "${PROJECT_SOURCE_DIR}/include/konbini/render/hud_overlay_layer.h"
  )

  target_include_directories(
    konbini_hud_overlay
    PUBLIC
      "${PROJECT_SOURCE_DIR}/include"
  )

  target_compile_features(konbini_hud_overlay PUBLIC cxx_std_20)
  if(WIN32)
    target_compile_definitions(konbini_hud_overlay PRIVATE NOGDI)
  endif()
  target_link_libraries(
    konbini_hud_overlay
    PUBLIC
      ergo_render
      konbini_render_domain
    PRIVATE
      konbini_pictor_world_geometry
      pictor
      Vulkan::Vulkan
  )
  add_dependencies(konbini_hud_overlay konbini_shaders)
  konbini_enable_warnings(konbini_hud_overlay)
endif()
