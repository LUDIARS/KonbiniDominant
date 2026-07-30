function(konbini_verify_exact_git_source source_directory expected_revision)
  find_package(Git REQUIRED)

  execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${source_directory}" rev-parse HEAD
    RESULT_VARIABLE revision_result
    OUTPUT_VARIABLE actual_revision
    ERROR_VARIABLE revision_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(NOT revision_result EQUAL 0)
    message(FATAL_ERROR
      "Dependency source is not a readable git checkout: "
      "${source_directory}: ${revision_error}")
  endif()
  if(NOT actual_revision STREQUAL expected_revision)
    message(FATAL_ERROR
      "Dependency revision mismatch in ${source_directory}: "
      "expected ${expected_revision}, got ${actual_revision}")
  endif()

  execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${source_directory}"
            status --porcelain --untracked-files=normal
    RESULT_VARIABLE status_result
    OUTPUT_VARIABLE dirty_status
    ERROR_VARIABLE status_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(NOT status_result EQUAL 0)
    message(FATAL_ERROR
      "Unable to inspect dependency worktree state in "
      "${source_directory}: ${status_error}")
  endif()
  if(NOT dirty_status STREQUAL "")
    message(FATAL_ERROR
      "Dependency source must be clean at ${expected_revision}; "
      "dirty source rejected: ${source_directory}")
  endif()
endfunction()
