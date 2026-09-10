# Native mobile build (implementation baseline)
Status: Android / iOS builds and device launches NOT VERIFIED.
Date: 2026-09-09

## Shared behavior
Renderer stays Pictor, with the pinned Ergo frame composer and Figmentum city generator.
Android uses Vulkan and ANativeWindow. iOS uses Pictor Vulkan through MoltenVK and CAMetalLayer.
GameSession owns the same Phase 1–4 rules on Windows and mobile.
Tap selects/builds; drag pans; two fingers zoom. Chain, skill and action choices use the shared touch HUD.
Landscape is declared in both application manifests. iOS draws inside the safe area.
Background/focus loss pauses ticks and cancels contacts. Android window recreation reuploads geometry while retaining the match.

## Android Studio
Open mobile/android. This is a native C++ NativeActivity application, arm64-v8a only.
Prerequisites: JDK 17, Gradle 8.11.1, Android SDK 35, NDK 27.2.12479018, CMake 3.31.1 (>=3.28 required by this repository).
The Gradle wrapper is not bundled. Install Gradle 8.11.1, then run 'gradle wrapper --gradle-version 8.11.1' in mobile/android before opening/syncing.
Configure sdk.dir and, if using an external CMake, cmake.dir in mobile/android/local.properties.
Install a host Vulkan SDK / glslc and set VULKAN_SDK. glslc runs on the build host; target Vulkan comes from the NDK.
Git access to the three fixed dependency repositories is required for FetchContent.
Build with 'gradlew.bat :app:assembleDebug' on Windows or './gradlew :app:assembleDebug' on macOS/Linux.
Output: mobile/android/app/build/outputs/apk/debug/app-debug.apk.
Supported device declaration: Android 10+ with Vulkan 1.2, arm64, touch screen. Actual supported GPU list is not established.
The APK contains content JSON and six compiled SPIR-V shaders. NativeActivity extracts these into private application storage before startup.
CMake stages assets before the Gradle merge-assets task. Rebuild after changing content or shaders.
Release signing is not configured. No Play Store publication was performed.

## Xcode / iOS
A Mac, Xcode with iOS SDK, CMake >=3.28, host glslc, and a matching MoltenVK static library + Vulkan headers are required.
Use MoltenVK.xcframework's device arm64 slice, not a macOS or simulator library.
From the repository root:
cmake -S . -B build-ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 -DKONBINI_BUILD_TESTS=OFF -DKONBINI_MOLTENVK_LIBRARY=/absolute/path/to/ios-arm64/libMoltenVK.a -DKONBINI_VULKAN_HEADERS=/absolute/path/to/Vulkan-Headers/include -DKONBINI_APPLE_TEAM=YOUR_TEAM
Open build-ios/KonbiniDominant.xcodeproj. Select konbini_mobile and the connected device; select the signing team, then build.
For simulator builds, use a separate build directory, iphonesimulator SDK, and the matching MoltenVK simulator slice.
The bundle stages content JSON and SPIR-V next to the executable. The application uses UIKit lifecycle, CAMetalLayer, and landscape-only orientation.
A game-owned creation adapter enables the portability extensions that MoltenVK advertises. It is linked only for iOS; the Pictor checkout remains pinned and unmodified.
No IPA, signing, simulator run, or physical-device run has been performed.

## Outstanding verification
- Android Studio Gradle sync, NDK compile/link, APK asset inspection.
- Xcode compile/link, signing, bundle resources, MoltenVK device/simulator compatibility.
- Native startup, actual picture, landscape rotation, safe-area/notch/navigation-bar layout.
- Tap/build/skill selection, drag/pinch, cancellation and background/foreground recovery on both platforms.
- Phase 1–4 clear and sustained performance / memory behavior on actual phones.
- Small-screen HUD readability and thermal load. Desktop compilation does not verify these.

## Sources
Android native Vulkan: https://developer.android.com/ndk/guides/graphics/
Android Gradle plugin: https://developer.android.com/build/releases/about-agp
MoltenVK runtime requirements: https://github.com/KhronosGroup/MoltenVK/blob/main/Docs/MoltenVK_Runtime_UserGuide.md
Vulkan portability: https://docs.vulkan.org/spec/latest/chapters/initialization.html
Apple supported orientations: https://developer.apple.com/documentation/bundleresources/information-property-list/uisupportedinterfaceorientations
