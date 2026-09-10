# Target Vulkan is supplied by MobileVulkan; never discover the host loader.
if(NOT TARGET Vulkan::Vulkan OR NOT TARGET Vulkan::glslc)
  message(FATAL_ERROR "Mobile Vulkan targets must be configured before dependencies")
endif()
set(Vulkan_FOUND TRUE)
set(Vulkan_glslc_FOUND TRUE)
set(Vulkan_GLSLC_EXECUTABLE "${KONBINI_HOST_GLSLC}")
