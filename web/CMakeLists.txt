# Browser host: shared authoritative C++ gameplay, actual pinned Pictor WebGL2.
set(KONBINI_BUILD_RENDER OFF)
set(KONBINI_BUILD_FIGMENTUM_ADAPTER OFF)
add_compile_options(-fexceptions)
add_link_options(-fexceptions)
include("${CMAKE_CURRENT_SOURCE_DIR}/dependencies.cmake")
add_subdirectory("${PROJECT_SOURCE_DIR}/src" "${CMAKE_BINARY_DIR}/src")
add_library(konbini_web_presentation STATIC
  ../src/app/game_session.cpp
  ../src/app/frame_presenter.cpp
  ../src/adapters/ergo/construction_presentation.cpp
  ../src/adapters/ergo/construction_particle_presets.cpp
  ../src/adapters/ergo/construction_particle_geometry.cpp)
target_link_libraries(konbini_web_presentation PUBLIC konbini_app_domain ergo_particle)
konbini_enable_warnings(konbini_web_presentation)

add_executable(konbini_web main.cpp browser_input.cpp pictor_web_renderer.cpp)
target_link_libraries(konbini_web PRIVATE konbini_web_presentation pictor_webgl)
target_link_options(konbini_web PRIVATE
  -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2
  -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=536870912
  -sSTACK_SIZE=1048576 -sENVIRONMENT=web
  -sEXPORTED_RUNTIME_METHODS=FS,UTF8ToString
  "SHELL:--preload-file ${PROJECT_SOURCE_DIR}/data/content/first-playable.json@/data/content/first-playable.json"
  "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/shell.html")
set_target_properties(konbini_web PROPERTIES
  OUTPUT_NAME index SUFFIX ".html" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist"
  LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shell.html;${PROJECT_SOURCE_DIR}/data/content/first-playable.json")
find_package(Git REQUIRED)
add_custom_command(TARGET konbini_web POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/browser.js" "${CMAKE_BINARY_DIR}/dist/browser.js"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/style.css" "${CMAKE_BINARY_DIR}/dist/style.css"
  COMMAND "${CMAKE_COMMAND}" "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
    "-DREPOSITORY_DIR=${PROJECT_SOURCE_DIR}"
    "-DOUTPUT_FILE=${CMAKE_BINARY_DIR}/dist/BUILD_REVISION.txt"
    -P "${PROJECT_SOURCE_DIR}/cmake/WriteGitRevision.cmake" VERBATIM)
set_property(TARGET konbini_web APPEND PROPERTY LINK_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/browser.js" "${CMAKE_CURRENT_SOURCE_DIR}/style.css")
add_dependencies(konbini_review konbini_web)
konbini_enable_warnings(konbini_web)
