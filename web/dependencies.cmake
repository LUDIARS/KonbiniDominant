# Compile the upstream optional WebGL module directly. Its root CMake also
# configures desktop GLFW/Vulkan; neither is a browser dependency.
function(konbini_web_fetch name revision)
  FetchContent_Declare(${name}
    GIT_REPOSITORY "https://github.com/LUDIARS/${name}.git"
    GIT_TAG "${revision}" GIT_SHALLOW FALSE)
  FetchContent_GetProperties(${name})
  if(NOT ${name}_POPULATED)
    FetchContent_Populate(${name})
  endif()
  konbini_verify_exact_git_source("${${name}_SOURCE_DIR}" "${revision}")
  set("${name}_SOURCE_DIR" "${${name}_SOURCE_DIR}" PARENT_SCOPE)
endfunction()
konbini_web_fetch(pictor c088e8d1b7b9e2625b7a8d923c89d4d684566c16)
konbini_web_fetch(ergo 771b027f0e5492015b27f54c3bab1fd5c1ae4790)
add_library(pictor_webgl STATIC
  "${pictor_SOURCE_DIR}/src/core/types.cpp"
  "${pictor_SOURCE_DIR}/src/webgl/webgl_context.cpp"
  "${pictor_SOURCE_DIR}/src/webgl/webgl_shader.cpp"
  "${pictor_SOURCE_DIR}/src/webgl/webgl_buffer.cpp"
  "${pictor_SOURCE_DIR}/src/webgl/webgl_renderer.cpp")
target_include_directories(pictor_webgl PUBLIC "${pictor_SOURCE_DIR}/include")
target_compile_definitions(pictor_webgl PUBLIC PICTOR_HAS_WEBGL=1)
add_library(ergo_particle STATIC
  "${ergo_SOURCE_DIR}/src/particle/effect_config.cpp"
  "${ergo_SOURCE_DIR}/src/particle/particle_system.cpp")
target_include_directories(ergo_particle PUBLIC "${ergo_SOURCE_DIR}/include")
