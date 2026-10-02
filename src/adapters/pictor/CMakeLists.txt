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

# GPU 常駐 geometry と shader module / world pipeline の所有者。Pictor の
# registry / context には依存せず素の Vulkan だけを使うので、scene targets
# とは別ライブラリにする。
add_library(
  konbini_pictor_world_geometry
  STATIC
    host_visible_memory.cpp
    spirv_module.cpp
    world_geometry_buffer.cpp
    world_overlay_buffers.cpp
    world_pipeline_factory.cpp
    "${PROJECT_SOURCE_DIR}/include/konbini/adapters/pictor/host_visible_memory.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/adapters/pictor/spirv_module.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/adapters/pictor/world_geometry_buffer.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/adapters/pictor/world_overlay_buffers.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/adapters/pictor/world_pipeline_factory.h"
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
)
konbini_enable_warnings(konbini_pictor_world_geometry)

# Gate 5 production bridge (spec/interface/pictor-rendering.md Required
# bridge): GpuAssetStore / KonbiniBatchGpuSource / PictorFrameBridge と、
# それらが使う Pictor の SceneRegistry / BatchBuilder / CompiledBatchRecorder。
# scene sync / batch plan / asset bookkeeping は device を作らずに test できる
# よう、Vulkan 実体の owner (uploader / instance buffers / pipelines) と
# ファイルを分けている。
add_library(
  konbini_pictor_bridge
  STATIC
    gpu_asset_store.cpp
    konbini_batch_gpu_source.cpp
    pictor_batch_plan.cpp
    pictor_frame_bridge.cpp
    pictor_scene_sync.cpp
    vulkan_gpu_mesh_uploader.cpp
    world_frustum.cpp
    world_geometry_loader.cpp
    world_instance_buffers.cpp
    world_instanced_pipelines.cpp
    "${PROJECT_SOURCE_DIR}/include/konbini/adapters/pictor/gpu_asset_store.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/adapters/pictor/gpu_mesh_uploader.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/adapters/pictor/konbini_batch_gpu_source.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/adapters/pictor/pictor_batch_plan.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/adapters/pictor/pictor_frame_bridge.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/adapters/pictor/pictor_scene_sync.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/adapters/pictor/vulkan_gpu_mesh_uploader.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/adapters/pictor/world_frustum.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/adapters/pictor/world_geometry_loader.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/adapters/pictor/world_instance_buffers.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/adapters/pictor/world_instanced_pipelines.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/adapters/pictor/world_shader_keys.h"
)

target_include_directories(
  konbini_pictor_bridge
  PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

target_compile_features(konbini_pictor_bridge PUBLIC cxx_std_20)
# Pictor の core / registry header は PassType::OPAQUE と
# ObjectFlags::TRANSPARENT を持ち、Win32 GDI macro と衝突する。
if(WIN32)
  target_compile_definitions(konbini_pictor_bridge PRIVATE NOGDI)
endif()
target_link_libraries(
  konbini_pictor_bridge
  PUBLIC
    pictor
    konbini_pictor_world_geometry
    konbini_render_domain
    Vulkan::Vulkan
  PRIVATE
    konbini_city
)
konbini_enable_warnings(konbini_pictor_bridge)
