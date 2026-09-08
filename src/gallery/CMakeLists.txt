add_executable(konbini_store_gallery main.cpp frame_export.cpp frame_export.h
  reference_store_geometry.cpp reference_store_geometry.h)
target_compile_features(konbini_store_gallery PRIVATE cxx_std_20)
target_compile_definitions(konbini_store_gallery PRIVATE
  NOGDI "KONBINI_GALLERY_SHADER_DIR=\"${CMAKE_BINARY_DIR}/shaders\""
  "KONBINI_GALLERY_OUTPUT_DIR=\"${CMAKE_BINARY_DIR}/store-gallery\"")
target_link_libraries(konbini_store_gallery PRIVATE konbini_ergo_adapter glfw)
# No RUNTIME_OUTPUT_DIRECTORY override: like konbini_dominant, the multi-config
# generator places the exe in build/<CONFIG>/, which is the path the
# kd-store-gallery Excubitor entry executes.
add_dependencies(konbini_store_gallery konbini_shaders)
konbini_enable_warnings(konbini_store_gallery)
