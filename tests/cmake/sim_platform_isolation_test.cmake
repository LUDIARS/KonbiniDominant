# Script-mode test: konbini_sim must not include OS, Pictor, Ergo, Vulkan or
# Figmentum headers, nor link their targets. Mobile hosts reuse the same
# simulation, so a platform header here would fork the rule per platform.
# @implements spec/interface/mobile-platform.md Required boundaries
# @implements spec/design.md 5. Tick と決定性

if(NOT DEFINED KONBINI_SOURCE_DIR)
  message(FATAL_ERROR "KONBINI_SOURCE_DIR must be passed with -D")
endif()

file(GLOB_RECURSE sim_sources
  "${KONBINI_SOURCE_DIR}/include/konbini/sim/*.h"
  "${KONBINI_SOURCE_DIR}/src/sim/*.h"
  "${KONBINI_SOURCE_DIR}/src/sim/*.cpp"
)
list(LENGTH sim_sources sim_source_count)
if(sim_source_count EQUAL 0)
  message(FATAL_ERROR "FAIL no konbini_sim sources found under ${KONBINI_SOURCE_DIR}")
endif()

# Matched against the include target with the game's own `konbini/` headers
# removed first (`konbini/sim/figmentum_facility_key.h` is a game-owned key).
set(forbidden_include_regex
  "(^|/)(vulkan|pictor|ergo|GLFW|glfw|figmentum|android|jni\\.h|UIKit|Metal|QuartzCore|Foundation|AppKit|windows\\.h|Windows\\.h|unistd\\.h|dlfcn\\.h)")

set(violations "")
foreach(source IN LISTS sim_sources)
  file(STRINGS "${source}" include_lines REGEX "^[ \t]*#[ \t]*(include|import)")
  foreach(line IN LISTS include_lines)
    string(REGEX REPLACE "^[ \t]*#[ \t]*(include|import)[ \t]*[<\"]([^>\"]*)[>\"].*$" "\\2" target "${line}")
    # Raw touch / gesture values and platform adapters live in app and
    # adapters; the simulation only sees normalized PlayerCommands.
    if(target MATCHES "^konbini/(app|adapters)/")
      file(RELATIVE_PATH relative "${KONBINI_SOURCE_DIR}" "${source}")
      list(APPEND violations "${relative}: ${target}")
      continue()
    endif()
    if(target MATCHES "^konbini/")
      continue()
    endif()
    if(target MATCHES "${forbidden_include_regex}")
      file(RELATIVE_PATH relative "${KONBINI_SOURCE_DIR}" "${source}")
      list(APPEND violations "${relative}: ${target}")
    endif()
  endforeach()
endforeach()

# Only the link lists: source lists legitimately name game-owned headers such
# as `figmentum_facility_key.h`.
file(READ "${KONBINI_SOURCE_DIR}/src/sim/CMakeLists.txt" sim_cmake)
string(REGEX MATCHALL "target_link_libraries\\([^)]*\\)" sim_links "${sim_cmake}")
foreach(link IN LISTS sim_links)
  if(link MATCHES "(pictor|ergo|Vulkan|glfw|figmentum)")
    list(APPEND violations "src/sim/CMakeLists.txt links ${CMAKE_MATCH_1}")
  endif()
endforeach()

if(violations)
  string(REPLACE ";" "\n  " report "${violations}")
  message(FATAL_ERROR "FAIL konbini_sim platform isolation:\n  ${report}")
endif()
message(STATUS "konbini_sim_platform_isolation_tests: ${sim_source_count} sources checked")
