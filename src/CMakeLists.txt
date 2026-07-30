add_subdirectory(sim)

# Stable aggregate used by the deterministic-primitives local review.
# add_custom_target(... DEPENDS) takes files / custom-command outputs; a
# target-level dependency has to go through add_dependencies().
add_custom_target(konbini_review)
add_dependencies(konbini_review konbini_sim)
