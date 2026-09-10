add_library(konbini_mobile_game STATIC
  ../src/app/game_session.cpp
  ../src/app/frame_presenter.cpp
  ../src/app/native_mobile_runtime.cpp)
target_link_libraries(konbini_mobile_game PUBLIC konbini_app_domain konbini_figmentum_adapter
  konbini_ergo_adapter konbini_pictor_world_geometry konbini_hud_overlay konbini_world_render)
konbini_enable_warnings(konbini_mobile_game)
if(ANDROID)
  enable_language(C)
  add_library(konbini_native_glue STATIC "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c")
  target_include_directories(konbini_native_glue PUBLIC "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue")
  add_library(konbini_mobile SHARED android/native_main.cpp)
  target_link_libraries(konbini_mobile PRIVATE konbini_mobile_game konbini_native_glue pictor android log Vulkan::Vulkan)
  target_link_options(konbini_mobile PRIVATE "-uANativeActivity_onCreate")
  add_dependencies(konbini_mobile konbini_shaders konbini_runtime_content)
  # Gradle may build Debug and Release concurrently. Keep their generated
  # assets separate so one configuration cannot package the other's shaders.
  set(asset_dir "${PROJECT_SOURCE_DIR}/build-mobile-assets/$<LOWER_CASE:$<CONFIG>>")
  add_custom_target(konbini_mobile_assets
    COMMAND "${CMAKE_COMMAND}"
      "-DKONBINI_ASSET_SOURCE_CONTENT=${konbini_runtime_content_file}"
      "-DKONBINI_ASSET_SOURCE_SHADERS=${CMAKE_BINARY_DIR}/shaders"
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
      "-DKONBINI_ASSET_SOURCE_CONTENT=${konbini_runtime_content_file}"
      "-DKONBINI_ASSET_SOURCE_SHADERS=${CMAKE_BINARY_DIR}/shaders"
      "-DKONBINI_ASSET_TARGET_DIRECTORY=$<TARGET_BUNDLE_DIR:konbini_mobile>"
      -P "${PROJECT_SOURCE_DIR}/cmake/StageRuntimeAssets.cmake"
    DEPENDS konbini_shaders konbini_runtime_content VERBATIM)
  add_dependencies(konbini_mobile konbini_mobile_assets)
endif()
konbini_enable_warnings(konbini_mobile)
add_dependencies(konbini_review konbini_mobile konbini_runtime_content)
