add_executable(
  konbini_sim_tests
  sim/deterministic_primitives_test.cpp
)

target_link_libraries(konbini_sim_tests PRIVATE konbini_sim)
konbini_enable_warnings(konbini_sim_tests)

add_test(NAME konbini_sim_tests COMMAND konbini_sim_tests)

# Keep the local review aggregate honest: with tests enabled it must also cover
# the test binary, not just the library.
add_dependencies(konbini_review konbini_sim_tests)
