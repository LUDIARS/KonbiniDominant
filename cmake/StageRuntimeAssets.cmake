foreach(required_variable IN ITEMS
    KONBINI_ASSET_SOURCE_CONTENT
    KONBINI_ASSET_SOURCE_SHADERS
    KONBINI_ASSET_TARGET_DIRECTORY)
  if(NOT DEFINED ${required_variable})
    message(FATAL_ERROR "${required_variable} is required")
  endif()
endforeach()

cmake_path(ABSOLUTE_PATH KONBINI_ASSET_SOURCE_CONTENT NORMALIZE
  OUTPUT_VARIABLE source_content)
cmake_path(ABSOLUTE_PATH KONBINI_ASSET_SOURCE_SHADERS NORMALIZE
  OUTPUT_VARIABLE source_shaders)
cmake_path(ABSOLUTE_PATH KONBINI_ASSET_TARGET_DIRECTORY NORMALIZE
  OUTPUT_VARIABLE target_directory)
set(target_content "${target_directory}/data/content/first-playable.json")
set(target_shaders "${target_directory}/shaders")

if(NOT source_content STREQUAL target_content)
  file(MAKE_DIRECTORY "${target_directory}/data/content")
  file(COPY_FILE "${source_content}" "${target_content}" ONLY_IF_DIFFERENT)
endif()

if(NOT source_shaders STREQUAL target_shaders)
  # A renamed or removed shader must not survive in a later package.
  file(REMOVE_RECURSE "${target_shaders}")
  file(MAKE_DIRECTORY "${target_shaders}")
  file(COPY "${source_shaders}/" DESTINATION "${target_shaders}")
endif()
