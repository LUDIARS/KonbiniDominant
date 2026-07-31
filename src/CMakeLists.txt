add_subdirectory(sim)
add_subdirectory(city)
if(KONBINI_BUILD_RENDER)
  add_subdirectory(adapters/pictor)
endif()
add_subdirectory(render)
if(KONBINI_BUILD_FIGMENTUM_ADAPTER)
  add_subdirectory(adapters/figmentum)
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
)
if(KONBINI_BUILD_FIGMENTUM_ADAPTER)
  add_dependencies(konbini_review konbini_figmentum_adapter)
endif()
if(KONBINI_BUILD_RENDER)
  add_dependencies(
    konbini_review
    konbini_pictor_scene_targets
    konbini_world_composite
  )
endif()
