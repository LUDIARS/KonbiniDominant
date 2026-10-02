add_library(konbini_mobile_game STATIC
  ../src/app/game_session.cpp
  ../src/app/native_game_session.cpp
  ../src/app/frame_presenter.cpp
  ../src/app/native_mobile_runtime.cpp)
target_link_libraries(konbini_mobile_game PUBLIC konbini_app_domain konbini_figmentum_adapter
  konbini_ergo_adapter konbini_pictor_bridge konbini_hud_overlay konbini_world_render)
konbini_enable_warnings(konbini_mobile_game)
# Same staged locations src/app (content) and src/render (SPIR-V) build into.
# Those variables live in their directory scope, so name the paths here.
set(konbini_mobile_content_file "${CMAKE_BINARY_DIR}/data/content/first-playable.json")
set(konbini_mobile_shader_directory "${CMAKE_BINARY_DIR}/shaders")
if(ANDROID)
  enable_language(C)
  # NDK libc++ 18 lacks floating-point std::from_chars, which the pinned
  # Figmentum garment profile and Pictor visus JSON call. Force-include the
  # shim into those dependency targets only; their sources stay pinned. The
  # definitions are compiled into konbini_mobile itself so the dependencies'
  # install export sets gain no game-side target.
  foreach(dependency_target IN ITEMS figmentum_garment figmentum_core pictor)
    if(NOT TARGET ${dependency_target})
      message(FATAL_ERROR "Android charconv shim target ${dependency_target} is missing")
    endif()
    target_compile_options(${dependency_target} PRIVATE
      "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/android/compat/libcxx_float_from_chars.h")
  endforeach()
  add_library(konbini_native_glue STATIC "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c")
  target_include_directories(konbini_native_glue PUBLIC "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue")
  if(NOT CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
    # The first package ABI (spec/setup/mobile-development.md#Android). Another
    # ABI is a separate decision (TBD-MOBILE-OS-01), not a silent extra build.
    message(FATAL_ERROR "KonbiniDominant Android supports arm64-v8a only, got '${CMAKE_ANDROID_ARCH_ABI}'")
  endif()
  # Thin NativeActivity host split by responsibility: looper entry, lifecycle /
  # window owner, touch normalization, APK asset reader, thermal listener.
  add_library(konbini_mobile SHARED
    android/native_main.cpp
    android/android_host.cpp
    android/android_touch_input.cpp
    android/android_asset_reader.cpp
    android/android_thermal_monitor.cpp
    android/compat/libcxx_float_from_chars.cpp)
  # Runtime graphics is the NDK `vulkan` library (MobileVulkan.cmake); the
  # host glslc only builds konbini_shaders and is never linked.
  target_link_libraries(konbini_mobile PRIVATE konbini_mobile_game konbini_native_glue pictor android log dl Vulkan::Vulkan)
  target_link_options(konbini_mobile PRIVATE "-uANativeActivity_onCreate")
  add_dependencies(konbini_mobile konbini_shaders konbini_runtime_content)
  # Gradle may build Debug and Release concurrently. Keep their generated
  # assets separate so one configuration cannot package the other's shaders.
  set(asset_dir "${PROJECT_SOURCE_DIR}/build-mobile-assets/$<LOWER_CASE:$<CONFIG>>")
  add_custom_target(konbini_mobile_assets
    COMMAND "${CMAKE_COMMAND}"
      "-DKONBINI_ASSET_SOURCE_CONTENT=${konbini_mobile_content_file}"
      "-DKONBINI_ASSET_SOURCE_SHADERS=${konbini_mobile_shader_directory}"
      "-DKONBINI_ASSET_TARGET_DIRECTORY=${asset_dir}"
      -P "${PROJECT_SOURCE_DIR}/cmake/StageRuntimeAssets.cmake"
    DEPENDS konbini_shaders konbini_runtime_content VERBATIM)
  add_dependencies(konbini_mobile konbini_mobile_assets)
else()
  enable_language(OBJCXX)
  # Restrict symbol adaptation to Pictor's compilation; the bridge calls the real loader.
  target_compile_definitions(pictor PRIVATE vkCreateInstance=konbiniCreateInstance vkCreateDevice=konbiniCreateDevice)
  add_executable(konbini_mobile MACOSX_BUNDLE ios/main.mm ios/vulkan_portability.cpp)
  target_compile_options(konbini_mobile PRIVATE "$<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>")
  target_link_libraries(konbini_mobile PRIVATE konbini_mobile_game pictor Vulkan::Vulkan
    "-framework UIKit" "-framework QuartzCore" "-framework Metal" "-framework Foundation"
    "-framework CoreGraphics" "-framework IOSurface")
  set_target_properties(konbini_mobile PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/ios/Info.plist"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.ludiars.konbinidominant"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "15.0"
    XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic")
  if(KONBINI_APPLE_TEAM)
    set_target_properties(konbini_mobile PROPERTIES XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${KONBINI_APPLE_TEAM}")
  endif()
  add_dependencies(konbini_mobile konbini_shaders konbini_runtime_content)
  add_custom_target(konbini_mobile_assets
    COMMAND "${CMAKE_COMMAND}"
      "-DKONBINI_ASSET_SOURCE_CONTENT=${konbini_mobile_content_file}"
      "-DKONBINI_ASSET_SOURCE_SHADERS=${konbini_mobile_shader_directory}"
      "-DKONBINI_ASSET_TARGET_DIRECTORY=$<TARGET_BUNDLE_DIR:konbini_mobile>"
      -P "${PROJECT_SOURCE_DIR}/cmake/StageRuntimeAssets.cmake"
    DEPENDS konbini_shaders konbini_runtime_content VERBATIM)
  add_dependencies(konbini_mobile konbini_mobile_assets)
endif()
konbini_enable_warnings(konbini_mobile)
add_dependencies(konbini_review konbini_mobile konbini_runtime_content)
