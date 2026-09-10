# Ergo runtime adapter: GLFW -> Ergo input injection, Pictor device / window
# ownership, and the FrameComposer pass graph with its swapchain recovery.
add_library(
  konbini_ergo_adapter
  STATIC
    layer_initialization_scope.cpp
    construction_presentation.cpp
    construction_particle_presets.cpp
    construction_particle_geometry.cpp
    render_device_host.cpp
    swapchain_identity.cpp
    tracked_render_layer.cpp
    world_frame_graph.cpp
    "${PROJECT_SOURCE_DIR}/include/konbini/adapters/ergo/ergo_input_bridge.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/adapters/ergo/input_action_map.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/adapters/ergo/layer_initialization_scope.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/adapters/ergo/render_device_host.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/adapters/ergo/swapchain_identity.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/adapters/ergo/tracked_render_layer.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/adapters/ergo/world_frame_graph.h"
)

if(NOT KONBINI_MOBILE)
  target_sources(konbini_ergo_adapter PRIVATE ergo_input_bridge.cpp native_touch_bridge.cpp input_action_map.cpp)
endif()

target_include_directories(
  konbini_ergo_adapter
  PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

target_compile_features(konbini_ergo_adapter PUBLIC cxx_std_20)
if(WIN32)
  target_compile_definitions(konbini_ergo_adapter PRIVATE NOGDI)
  target_link_libraries(konbini_ergo_adapter PRIVATE comctl32)
endif()

target_link_libraries(
  konbini_ergo_adapter
  PUBLIC
    ergo_input
    ergo_render
    konbini_app_domain
    konbini_hud_overlay
    konbini_pictor_scene_targets
    konbini_pictor_world_geometry
    konbini_world_composite
    konbini_world_render
  PRIVATE
    pictor
    ergo_particle
    Vulkan::Vulkan
)

konbini_enable_warnings(konbini_ergo_adapter)
