add_library(
  konbini_pictor_scene_targets
  STATIC
    world_scene_targets.cpp
    "${PROJECT_SOURCE_DIR}/include/konbini/adapters/pictor/world_scene_targets.h"
)

target_include_directories(
  konbini_pictor_scene_targets
  PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

target_compile_features(konbini_pictor_scene_targets PUBLIC cxx_std_20)
# Pictor's registry headers parse PassType::OPAQUE / ObjectFlags::TRANSPARENT,
# which collide with the Win32 GDI macros. The public header also defines
# NOGDI, but only when it is included first; define it target-wide so include
# order inside this library cannot regress. Matches konbini_world_composite.
if(WIN32)
  target_compile_definitions(konbini_pictor_scene_targets PRIVATE NOGDI)
endif()
target_link_libraries(
  konbini_pictor_scene_targets
  PUBLIC
    pictor
    Vulkan::Vulkan
)
konbini_enable_warnings(konbini_pictor_scene_targets)

# GPU 常駐 geometry と shader module の所有者。Pictor の registry / context に
# は依存せず素の Vulkan だけを使うので、scene targets とは別ライブラリにする。
add_library(
  konbini_pictor_world_geometry
  STATIC
    spirv_module.cpp
    world_geometry_buffer.cpp
    world_geometry_cache.cpp
    world_geometry_loader.cpp
    world_overlay_buffers.cpp
    "${PROJECT_SOURCE_DIR}/include/konbini/adapters/pictor/spirv_module.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/adapters/pictor/world_geometry_buffer.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/adapters/pictor/world_geometry_cache.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/adapters/pictor/world_geometry_loader.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/adapters/pictor/world_overlay_buffers.h"
)

target_include_directories(
  konbini_pictor_world_geometry
  PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

target_compile_features(konbini_pictor_world_geometry PUBLIC cxx_std_20)
if(WIN32)
  target_compile_definitions(konbini_pictor_world_geometry PRIVATE NOGDI)
endif()
target_link_libraries(
  konbini_pictor_world_geometry
  PUBLIC
    konbini_render_domain
    Vulkan::Vulkan
  PRIVATE
    konbini_city
)
konbini_enable_warnings(konbini_pictor_world_geometry)
