# Script-mode test: the Pictor / Ergo / Figmentum revisions that CMake fetches
# and verifies with konbini_verify_exact_git_source() are the revisions the
# setup contract and README publish, and the Figmentum revision the city
# manifest accepts. A pin bump that forgets one of them fails here.
# @implements spec/setup/native-development.md Dependency revisions
# @implements spec/interface/pictor-rendering.md Surface / device recovery
# @implements spec/setup/macos-development.md Dependency revisions

set(root "${CMAKE_CURRENT_LIST_DIR}/../..")
file(READ "${root}/CMakeLists.txt" build)
file(READ "${root}/src/adapters/figmentum/CMakeLists.txt" figmentum_build)

function(read_pin out text variable)
  string(REGEX MATCH "set\\([ \t\r\n]*${variable}[ \t\r\n]*\"([0-9a-f]+)\"" match "${text}")
  # Copy before any other MATCHES resets CMAKE_MATCH_1.
  set(revision "${CMAKE_MATCH_1}")
  string(LENGTH "${revision}" length)
  if(NOT length EQUAL 40)
    message(FATAL_ERROR "FAIL: ${variable} must be a literal 40-hex revision")
  endif()
  set(${out} "${revision}" PARENT_SCOPE)
endfunction()

read_pin(pictor "${build}" KONBINI_PICTOR_REVISION)
read_pin(ergo "${build}" KONBINI_ERGO_REVISION)
read_pin(figmentum "${figmentum_build}" KONBINI_FIGMENTUM_REVISION)

foreach(document spec/setup/native-development.md README.md)
  file(READ "${root}/${document}" text)
  foreach(entry "Pictor|${pictor}" "Ergo|${ergo}" "Figmentum|${figmentum}")
    string(REPLACE "|" ";" entry "${entry}")
    list(GET entry 0 name)
    list(GET entry 1 revision)
    if(NOT text MATCHES "\\| ${name} \\| `${revision}` \\|")
      message(SEND_ERROR
        "FAIL: ${document} does not list ${name} ${revision} (CMake pin)")
    endif()
  endforeach()
endforeach()

# Exact-source verification is wired for every fetched dependency.
foreach(entry
    "build|KONBINI_PICTOR_REVISION"
    "build|KONBINI_ERGO_REVISION"
    "figmentum_build|KONBINI_FIGMENTUM_REVISION")
  string(REPLACE "|" ";" entry "${entry}")
  list(GET entry 0 text_variable)
  list(GET entry 1 variable)
  if(NOT "${${text_variable}}" MATCHES
      "konbini_verify_exact_git_source\\([^)]*\\$\\{${variable}\\}")
    message(SEND_ERROR "FAIL: ${variable} is not passed to konbini_verify_exact_git_source")
  endif()
endforeach()

# The manifest rejects plans from any other Figmentum revision, so the accepted
# revision must move with the fetched one.
file(READ "${root}/include/konbini/city/city_manifest.h" manifest_header)
if(NOT manifest_header MATCHES
    "kFigmentumRevision[ \t\r\n]*=[ \t\r\n]*\"${figmentum}\"")
  message(SEND_ERROR
    "FAIL: city::kFigmentumRevision does not match the Figmentum pin ${figmentum}")
endif()
