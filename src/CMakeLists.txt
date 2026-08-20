add_subdirectory(sim)
add_subdirectory(city)
if(KONBINI_BUILD_RENDER)
  add_subdirectory(adapters/pictor)
endif()
add_subdirectory(render)
if(KONBINI_BUILD_FIGMENTUM_ADAPTER)
  add_subdirectory(adapters/figmentum)
endif()
# app domain は render backend に依存しないので常に作る。ergo adapter と
# native executable は render 有効時だけ。
add_subdirectory(app)
if(KONBINI_BUILD_RENDER)
  add_subdirectory(adapters/ergo)
endif()

# Stable aggregate used by staged local reviews.
# add_custom_target(... DEPENDS) takes files / custom-command outputs; a
# target-level dependency has to go through add_dependencies().
add_custom_target(konbini_review)
add_dependencies(
  konbini_review
  konbini_sim
  konbini_city
  konbini_render_domain
  konbini_app_domain
)
if(KONBINI_BUILD_FIGMENTUM_ADAPTER)
  add_dependencies(konbini_review konbini_figmentum_adapter)
endif()
if(KONBINI_BUILD_RENDER)
  # world shader (SPIR-V) と offscreen render target / world pass の記録経路を
  # レビュー対象へ含める。konbini_shaders は custom target なので、
  # 依存を明示しないと staged review で SPIR-V が生成されない。
  add_dependencies(
    konbini_review
    konbini_shaders
    konbini_pictor_scene_targets
    konbini_pictor_world_geometry
    konbini_world_composite
    konbini_world_render
    konbini_hud_overlay
    konbini_ergo_adapter
  )
endif()
if(KONBINI_BUILD_RENDER AND KONBINI_BUILD_FIGMENTUM_ADAPTER)
  # native executable と、起動時に必要な shader / content を review 対象へ
  # 接続する (first-playable.md#PR acceptance)。
  add_dependencies(konbini_review konbini_dominant konbini_runtime_content)
endif()
