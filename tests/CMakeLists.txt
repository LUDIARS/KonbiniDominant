# One executable per test module. Each test file owns its own `main`, so the
# suites stay independent: a link error in the render geometry tests cannot
# hide the simulation ones, and ctest reports which module failed by name.
function(konbini_add_test name source library)
  add_executable(${name} ${source} check.h)
  target_link_libraries(${name} PRIVATE ${library})
  konbini_enable_warnings(${name})
  # @implements spec/test/presentation-validation-tests.md test binary
  # @implements spec/test/verification-strategy.md 原則
  add_test(NAME ${name} COMMAND ${name})
  # Keep the local review aggregate honest: with tests enabled it must also
  # cover the test binaries, not just the libraries.
  add_dependencies(konbini_review ${name})
endfunction()

konbini_add_test(
  konbini_sim_tests
  sim/deterministic_primitives_test.cpp
  konbini_sim
)
konbini_add_test(
  konbini_resident_presentation_content_tests
  sim/resident_presentation_content_test.cpp
  konbini_sim
)
konbini_add_test(
  konbini_resident_presentation_projection_tests
  sim/resident_presentation_projection_test.cpp
  konbini_sim
)
konbini_add_test(
  konbini_placement_cue_projection_tests
  sim/placement_cue_projection_test.cpp
  konbini_sim
)
add_executable(konbini_pedestrian_path_walking_tests sim/pedestrian_path_walking_test.cpp check.h)
target_link_libraries(konbini_pedestrian_path_walking_tests PRIVATE konbini_sim)
target_compile_definitions(konbini_pedestrian_path_walking_tests PRIVATE
  "KONBINI_PHASE1_CONTENT_FILE=\"${PROJECT_SOURCE_DIR}/data/content/phase1.json\"")
konbini_enable_warnings(konbini_pedestrian_path_walking_tests)
add_test(NAME konbini_pedestrian_path_walking_tests COMMAND konbini_pedestrian_path_walking_tests)
add_dependencies(konbini_review konbini_pedestrian_path_walking_tests)

konbini_add_test(
  konbini_presentation_schema_isolation_tests
  sim/presentation_schema_isolation_test.cpp
  konbini_sim
)

# The render-domain geometry is game-owned CPU data with no Vulkan dependency,
# so these tests build and run with KONBINI_BUILD_RENDER off as well.
konbini_add_test(
  konbini_visia_primitive_geometry_tests
  render/visia_primitive_geometry_test.cpp
  konbini_render_domain
)
konbini_add_test(
  konbini_speech_bubble_geometry_tests
  render/speech_bubble_geometry_test.cpp
  konbini_render_domain
)
konbini_add_test(
  konbini_store_placement_animation_tests
  render/store_placement_animation_test.cpp
  konbini_render_domain
)

add_executable(konbini_phase1_tests sim/phase1_game_loop_test.cpp)
target_link_libraries(konbini_phase1_tests PRIVATE konbini_app_domain konbini_render_domain)
target_compile_definitions(konbini_phase1_tests PRIVATE
  "KONBINI_PHASE1_CONTENT_FILE=\"${PROJECT_SOURCE_DIR}/data/content/phase1.json\""
  "KONBINI_CAMPAIGN_CONTENT_FILE=\"${PROJECT_SOURCE_DIR}/data/content/first-playable.json\"")
konbini_enable_warnings(konbini_phase1_tests)
add_test(NAME konbini_phase1_tests COMMAND konbini_phase1_tests)
add_dependencies(konbini_review konbini_phase1_tests)

add_executable(konbini_campaign_tests sim/campaign_systems_test.cpp check.h)
target_link_libraries(konbini_campaign_tests PRIVATE konbini_sim)
target_compile_definitions(konbini_campaign_tests PRIVATE
  "KONBINI_CAMPAIGN_CONTENT_FILE=\"${PROJECT_SOURCE_DIR}/data/content/first-playable.json\"")
konbini_enable_warnings(konbini_campaign_tests)
add_test(NAME konbini_campaign_tests COMMAND konbini_campaign_tests)
add_dependencies(konbini_review konbini_campaign_tests)

# app domain (fixed step / camera / selection / command / HUD) と HUD geometry。
# Vulkan にも Ergo にも依存しないので render 無効構成でも走る。
add_executable(
  konbini_app_tests
  app/first_playable_app_test.cpp
)

target_link_libraries(
  konbini_app_tests
  PRIVATE
    konbini_app_domain
    konbini_render_domain
    konbini_sim
)
target_compile_definitions(
  konbini_app_tests
  PRIVATE
    "KONBINI_TEST_CONTENT_FILE=\"${PROJECT_SOURCE_DIR}/data/content/first-playable.json\""
)
konbini_enable_warnings(konbini_app_tests)

add_test(NAME konbini_app_tests COMMAND konbini_app_tests)

# Keep the local review aggregate honest: with tests enabled it must also cover
# the test binaries, not just the libraries.
add_dependencies(konbini_review konbini_app_tests)

if(KONBINI_BUILD_RENDER)
  # Pictor typed FrameResult の分類、mobile render lifecycle、初期化失敗の
  # typed status、layer 初期化の逆順 rollback。
  # Vulkan の型を使うが device は作らない (起動テストではない)。
  add_executable(
    konbini_render_adapter_tests
    adapters/swapchain_recovery_test.cpp
  )

  target_link_libraries(
    konbini_render_adapter_tests
    PRIVATE
      konbini_ergo_adapter
      Vulkan::Vulkan
  )
  if(WIN32)
    target_compile_definitions(konbini_render_adapter_tests PRIVATE NOGDI)
  endif()
  konbini_enable_warnings(konbini_render_adapter_tests)

  add_test(
    NAME konbini_render_adapter_tests
    COMMAND konbini_render_adapter_tests
  )
  add_dependencies(konbini_review konbini_render_adapter_tests)

  # pinned Ergo の render backend 契約と、RenderContext の借用 handle 検査。
  # Vulkan device は作らない (起動テストではない)。
  add_executable(
    konbini_render_readiness_tests
    adapters/render_readiness_test.cpp
  )
  target_link_libraries(
    konbini_render_readiness_tests
    PRIVATE
      konbini_ergo_adapter
      pictor
      Vulkan::Vulkan
  )
  if(WIN32)
    target_compile_definitions(konbini_render_readiness_tests PRIVATE NOGDI)
  endif()
  konbini_enable_warnings(konbini_render_readiness_tests)
  add_test(
    NAME konbini_render_readiness_tests
    COMMAND konbini_render_readiness_tests
  )
  add_dependencies(konbini_review konbini_render_readiness_tests)

  # Gate 5 bridge: GpuAssetStore lifetime、IBatchGpuSource resolution、
  # FacilityId <-> ObjectId 差分、Pictor 実 SceneRegistry / BatchBuilder を
  # 通した instance path。Vulkan device は作らない (起動テストではない)。
  add_executable(
    konbini_pictor_bridge_tests
    adapters/pictor_bridge_test.cpp
    check.h
  )
  target_link_libraries(
    konbini_pictor_bridge_tests
    PRIVATE
      konbini_pictor_bridge
  )
  if(WIN32)
    target_compile_definitions(konbini_pictor_bridge_tests PRIVATE NOGDI)
  endif()
  konbini_enable_warnings(konbini_pictor_bridge_tests)
  add_test(
    NAME konbini_pictor_bridge_tests
    COMMAND konbini_pictor_bridge_tests
  )
  add_dependencies(konbini_review konbini_pictor_bridge_tests)
endif()

# configure 時の Ergo real render 契約 (desktop / Android / iOS の選択)。
# toolchain 不要の script mode で評価する。
# CMake の Pictor / Ergo pin と setup spec / README の依存表の一致。
add_test(
  NAME konbini_dependency_pin_contract_tests
  COMMAND "${CMAKE_COMMAND}" -P
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependency_pin_contract_test.cmake"
)

add_test(
  NAME konbini_ergo_real_render_contract_tests
  COMMAND "${CMAKE_COMMAND}" -P
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ergo_real_render_contract_test.cmake"
)

add_executable(konbini_grid_town_ui_tests render/grid_town_ui_test.cpp)
target_link_libraries(konbini_grid_town_ui_tests PRIVATE konbini_app_domain konbini_render_domain)
target_compile_definitions(konbini_grid_town_ui_tests PRIVATE
  "KONBINI_GRID_CONTENT_FILE=\"${PROJECT_SOURCE_DIR}/data/content/first-playable.json\"")
konbini_enable_warnings(konbini_grid_town_ui_tests)
add_test(NAME konbini_grid_town_ui_tests COMMAND konbini_grid_town_ui_tests)
add_dependencies(konbini_review konbini_grid_town_ui_tests)

if(KONBINI_BUILD_RENDER)
  add_executable(konbini_construction_tests adapters/construction_presentation_test.cpp)
  target_link_libraries(konbini_construction_tests PRIVATE konbini_ergo_adapter)
  target_compile_definitions(konbini_construction_tests PRIVATE
    "KONBINI_CONSTRUCTION_CONTENT_FILE=\"${PROJECT_SOURCE_DIR}/data/content/phase1.json\"")
  konbini_enable_warnings(konbini_construction_tests)
  add_test(NAME konbini_construction_tests COMMAND konbini_construction_tests)
  add_dependencies(konbini_review konbini_construction_tests)
endif()

# KD-MOB-003 mobile runtime / asset boundary. OS-, Vulkan- and Ergo-free, so
# these run with KONBINI_BUILD_RENDER off as well.
konbini_add_test(
  konbini_lifecycle_runtime_tests
  app/platform/lifecycle_runtime_test.cpp
  konbini_app_domain
)
konbini_add_test(
  konbini_display_metrics_tests
  app/platform/display_metrics_test.cpp
  konbini_app_domain
)
konbini_add_test(
  konbini_asset_boundary_tests
  app/platform/asset_boundary_test.cpp
  konbini_app_domain
)
konbini_add_test(
  konbini_derived_geometry_cache_tests
  city/derived_geometry_cache_policy_test.cpp
  konbini_city
)

# konbini_sim stays free of OS / Pictor / Ergo / Vulkan / Figmentum headers.
add_test(
  NAME konbini_sim_platform_isolation_tests
  COMMAND "${CMAKE_COMMAND}"
    "-DKONBINI_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sim_platform_isolation_test.cmake"
)

if(KONBINI_BUILD_FIGMENTUM_ADAPTER)
  # pinned Figmentum の CityPlan から pedestrian path contract を投影し、
  # manifest 検証・canonical hash・経路選択まで通す (KD-NPC-003)。
  add_executable(
    konbini_figmentum_pedestrian_path_tests
    adapters/figmentum_pedestrian_path_test.cpp
    check.h
  )
  target_link_libraries(
    konbini_figmentum_pedestrian_path_tests
    PRIVATE
      konbini_figmentum_adapter
      figmentum_core
  )
  konbini_enable_warnings(konbini_figmentum_pedestrian_path_tests)
  add_test(
    NAME konbini_figmentum_pedestrian_path_tests
    COMMAND konbini_figmentum_pedestrian_path_tests
  )
  add_dependencies(konbini_review konbini_figmentum_pedestrian_path_tests)
endif()
