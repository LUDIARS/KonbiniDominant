add_subdirectory(sim)
add_subdirectory(city)

# Stable aggregate used by staged local reviews.
# add_custom_target(... DEPENDS) takes files / custom-command outputs; a
# target-level dependency has to go through add_dependencies().
add_custom_target(konbini_review)
add_dependencies(konbini_review konbini_sim konbini_city)
