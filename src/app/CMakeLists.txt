# Pure application logic: no Vulkan, no Ergo, no GLFW. Keeping it in its own
# library lets the adapter layer depend on the game-owned FrameInput / HUD
# types without pulling the render backend into them.
add_library(
  konbini_app_domain
  STATIC
    playtest_options.cpp
    playtest_pilot.cpp
    playtest_basic_actions.cpp
    playtest_foreign_actions.cpp
    playtest_expansion_action.cpp
    playtest_report.cpp
    touch_contacts.cpp
    pointer_hud_text.cpp
    pointer_controls.cpp
    pointer_control_geometry.cpp
    pointer_input_controller.cpp
    selection_hud.cpp
    app_paths.cpp
    executable_directory.cpp
    camera_controller.cpp
    command_composer.cpp
    fixed_step_driver.cpp
    hud_text_model.cpp
    phase1_hud_text.cpp
    skill_text.cpp
    campaign_hud_text.cpp
    campaign_input_controller.cpp
    selection_controller.cpp
    simulation_host.cpp
    "${PROJECT_SOURCE_DIR}/include/konbini/app/app_paths.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/app/camera_controller.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/app/command_composer.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/app/fixed_step_driver.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/app/frame_input.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/app/hud_text_model.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/app/selection_controller.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/app/simulation_host.h"
)

target_include_directories(
  konbini_app_domain
  PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

target_compile_features(konbini_app_domain PUBLIC cxx_std_20)
target_link_libraries(
  konbini_app_domain
  PUBLIC
    konbini_city
    konbini_render_domain
    konbini_sim
)

# content は shader と同じく build が用意した runtime asset として扱う。
# repository の source を直接読むと、packaging 時に「実行に必要な資産」が
# build 出力に現れないままになる。
set(konbini_runtime_content_directory "${CMAKE_BINARY_DIR}/data/content")
set(
  konbini_runtime_content_file
  "${konbini_runtime_content_directory}/first-playable.json"
)
add_custom_command(
  OUTPUT "${konbini_runtime_content_file}"
  COMMAND
    "${CMAKE_COMMAND}" -E make_directory
    "${konbini_runtime_content_directory}"
  COMMAND
    "${CMAKE_COMMAND}" -E copy_if_different
    "${PROJECT_SOURCE_DIR}/data/content/first-playable.json"
    "${konbini_runtime_content_file}"
  DEPENDS "${PROJECT_SOURCE_DIR}/data/content/first-playable.json"
  VERBATIM
  COMMENT "Staging first-playable content"
)
add_custom_target(
  konbini_runtime_content
  DEPENDS "${konbini_runtime_content_file}"
)

# runtime が読む既定パス。shader は build が生成した SPIR-V を指し、上方探索
# fallback は使わない (別 build の古い .spv で起動してしまうため)。
# 実行時は KONBINI_SHADER_DIR / KONBINI_CONTENT_FILE で上書きできる。
target_compile_definitions(
  konbini_app_domain
  PRIVATE
    "KONBINI_DEFAULT_SHADER_DIR=\"shaders\""
    "KONBINI_DEFAULT_CONTENT_FILE=\"data/content/first-playable.json\""
)

konbini_enable_warnings(konbini_app_domain)

if(KONBINI_BUILD_RENDER AND KONBINI_BUILD_FIGMENTUM_ADAPTER AND NOT KONBINI_MOBILE)
  find_package(Git REQUIRED)
  add_executable(
    konbini_dominant
      app_runner.cpp
      frame_presenter.cpp
      game_session.cpp
      native_mobile_runtime.cpp
      main.cpp
      "${PROJECT_SOURCE_DIR}/include/konbini/app/app_runner.h"
      "${PROJECT_SOURCE_DIR}/include/konbini/app/frame_presenter.h"
  )

  set_target_properties(konbini_dominant PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  )
  target_compile_features(konbini_dominant PRIVATE cxx_std_20)
  if(WIN32)
    target_compile_definitions(konbini_dominant PRIVATE NOGDI)
  endif()
  target_link_libraries(
    konbini_dominant
    PRIVATE
      ergo_frame
      ergo_input
      konbini_app_domain
      konbini_ergo_adapter
      konbini_figmentum_adapter
      konbini_hud_overlay
      konbini_pictor_world_geometry
      konbini_world_render
  )
  # SPIR-V が無いと起動時に fail-fast するので、実行ファイルと同じ build で
  # 生成しておく。
  # TARGET_FILE_DIR does not add a dependency under CMP0112 (CMake >= 3.19).
  # Stage on every requested app build, including content-only changes.
  add_custom_target(konbini_app_assets
    COMMAND "${CMAKE_COMMAND}"
      "-DKONBINI_ASSET_SOURCE_CONTENT=${konbini_runtime_content_file}"
      "-DKONBINI_ASSET_SOURCE_SHADERS=${CMAKE_BINARY_DIR}/shaders"
      "-DKONBINI_ASSET_TARGET_DIRECTORY=$<TARGET_FILE_DIR:konbini_dominant>"
      -P "${PROJECT_SOURCE_DIR}/cmake/StageRuntimeAssets.cmake"
    DEPENDS konbini_shaders konbini_runtime_content
    VERBATIM
  )
  add_dependencies(konbini_dominant konbini_app_assets)
  # Bind a successful link to the checked-out source revision. The packager
  # rejects stale executables and user-supplied revision labels that disagree
  # with this post-build stamp.
  add_custom_command(TARGET konbini_dominant POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
      "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
      "-DREPOSITORY_DIR=${PROJECT_SOURCE_DIR}"
      "-DOUTPUT_FILE=$<TARGET_FILE_DIR:konbini_dominant>/BUILD_REVISION.txt"
      -P "${PROJECT_SOURCE_DIR}/cmake/WriteGitRevision.cmake"
    VERBATIM
  )
  konbini_enable_warnings(konbini_dominant)
endif()
