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
