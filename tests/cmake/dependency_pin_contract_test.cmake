# Script-mode test: the Pictor / Ergo revisions that CMake fetches and verifies
# with konbini_verify_exact_git_source() are the revisions the setup contract
# and README publish. A pin bump that forgets the spec fails here.
# @implements spec/setup/native-development.md Dependency revisions
# @implements spec/interface/pictor-rendering.md Surface / device recovery

set(root "${CMAKE_CURRENT_LIST_DIR}/../..")
file(READ "${root}/CMakeLists.txt" build)

function(read_pin out variable)
  string(REGEX MATCH "set\\([ \t\r\n]*${variable}[ \t\r\n]*\"([0-9a-f]+)\"" match "${build}")
  # Copy before any other MATCHES resets CMAKE_MATCH_1.
  set(revision "${CMAKE_MATCH_1}")
  string(LENGTH "${revision}" length)
  if(NOT length EQUAL 40)
    message(FATAL_ERROR "FAIL: ${variable} must be a literal 40-hex revision")
  endif()
  set(${out} "${revision}" PARENT_SCOPE)
endfunction()

read_pin(pictor KONBINI_PICTOR_REVISION)
read_pin(ergo KONBINI_ERGO_REVISION)

foreach(document spec/setup/native-development.md README.md)
  file(READ "${root}/${document}" text)
  foreach(entry "Pictor|${pictor}" "Ergo|${ergo}")
    string(REPLACE "|" ";" entry "${entry}")
    list(GET entry 0 name)
    list(GET entry 1 revision)
    if(NOT text MATCHES "\\| ${name} \\| `${revision}` \\|")
      message(SEND_ERROR
        "FAIL: ${document} does not list ${name} ${revision} (CMake pin)")
    endif()
  endforeach()
endforeach()

# Exact-source verification is wired for both fetched dependencies.
foreach(variable KONBINI_PICTOR_REVISION KONBINI_ERGO_REVISION)
  if(NOT build MATCHES "konbini_verify_exact_git_source\\([^)]*\\$\\{${variable}\\}")
    message(SEND_ERROR "FAIL: ${variable} is not passed to konbini_verify_exact_git_source")
  endif()
endforeach()
