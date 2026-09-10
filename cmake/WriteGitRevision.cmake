# @implements spec/setup/phase1-portable-build.md Build
if(NOT DEFINED GIT_EXECUTABLE OR NOT DEFINED REPOSITORY_DIR OR
   NOT DEFINED OUTPUT_FILE)
  message(FATAL_ERROR "revision stamp requires Git, repository, and output paths")
endif()

execute_process(
  COMMAND "${GIT_EXECUTABLE}" -C "${REPOSITORY_DIR}" rev-parse HEAD
  RESULT_VARIABLE revision_result
  OUTPUT_VARIABLE revision
  ERROR_VARIABLE revision_error
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(LENGTH "${revision}" revision_length)
if(NOT revision_result EQUAL 0 OR NOT revision_length EQUAL 40 OR
   NOT revision MATCHES "^[0-9a-f]+$")
  message(FATAL_ERROR "cannot stamp source revision: ${revision_error}")
endif()

execute_process(
  COMMAND "${GIT_EXECUTABLE}" -C "${REPOSITORY_DIR}" status --porcelain
  RESULT_VARIABLE status_result
  OUTPUT_VARIABLE status_output
  ERROR_VARIABLE status_error
)
if(NOT status_result EQUAL 0)
  message(FATAL_ERROR "cannot inspect source state: ${status_error}")
endif()
if(status_output STREQUAL "")
  set(source_state clean)
else()
  set(source_state dirty)
endif()

file(WRITE "${OUTPUT_FILE}" "${revision}\n${source_state}\n")
