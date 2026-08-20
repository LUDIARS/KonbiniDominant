add_executable(
  konbini_sim_tests
  sim/deterministic_primitives_test.cpp
)

target_link_libraries(konbini_sim_tests PRIVATE konbini_sim)
konbini_enable_warnings(konbini_sim_tests)

add_test(NAME konbini_sim_tests COMMAND konbini_sim_tests)

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
add_dependencies(konbini_review konbini_sim_tests konbini_app_tests)

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
