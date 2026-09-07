add_executable(konbini_store_gallery main.cpp frame_export.cpp frame_export.h
  reference_store_geometry.cpp reference_store_geometry.h)
target_compile_features(konbini_store_gallery PRIVATE cxx_std_20)
target_compile_definitions(konbini_store_gallery PRIVATE
  NOGDI "KONBINI_GALLERY_SHADER_DIR=\"${CMAKE_BINARY_DIR}/shaders\""
  "KONBINI_GALLERY_OUTPUT_DIR=\"${CMAKE_BINARY_DIR}/store-gallery\"")
target_link_libraries(konbini_store_gallery PRIVATE konbini_ergo_adapter glfw)
set_target_properties(konbini_store_gallery PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
add_dependencies(konbini_store_gallery konbini_shaders)
konbini_enable_warnings(konbini_store_gallery)
