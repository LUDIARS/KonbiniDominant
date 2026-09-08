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
  # swapchain out-of-date / device lost の分類と、layer 初期化の逆順 rollback。
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
endif()
