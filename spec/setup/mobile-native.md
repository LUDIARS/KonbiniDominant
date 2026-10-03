# Native mobile build (implementation baseline)
Status: Android debug APK build verified (KD-MOB-005, 2026-10-02). Android install / device launch and all iOS builds NOT VERIFIED.
Date: 2026-09-09 (Android package section updated 2026-10-02; iOS section rewritten for Metal 2026-10-03)

## Shared behavior
Renderer stays Pictor, with the pinned Ergo frame composer and Figmentum city generator.
Android uses Vulkan and ANativeWindow. iOS draws with Metal directly through Pictor MetalContext and CAMetalLayer (decided 2026-10-03; MoltenVK is no longer used for iOS).
GameSession owns the same Phase 1–4 rules on Windows and mobile.
Tap selects/builds; drag pans; two fingers zoom. Chain, skill and action choices use the shared touch HUD.
Landscape is declared in both application manifests. iOS draws inside the safe area.
Background/focus loss pauses ticks and cancels contacts. Android window recreation reuploads geometry while retaining the match.

## Android Studio
Open mobile/android. This is a native C++ NativeActivity application, arm64-v8a only.
Prerequisites: JDK 17, Gradle 8.11.1, Android SDK 35, NDK 27.2.12479018, CMake 3.31.1 (>=3.28 required by this repository).
The Gradle 8.11.1 wrapper is bundled (mobile/android/gradlew, gradlew.bat, gradle/wrapper/) with a pinned distribution checksum.
Configure sdk.dir (forward slashes, e.g. 'sdk.dir=E\:/Android/Sdk') and, if using an external CMake, cmake.dir in mobile/android/local.properties.
Installed versions and locations on the development machine: mobile-development.md, "Installed Android toolchain".
Install a host Vulkan SDK / glslc and set VULKAN_SDK. glslc runs on the build host; target Vulkan comes from the NDK.
Git access to the three fixed dependency repositories is required for FetchContent.
Build with 'gradlew.bat :app:assembleDebug' on Windows or './gradlew :app:assembleDebug' on macOS/Linux.
Output: mobile/android/app/build/outputs/apk/debug/app-debug.apk.
Supported device declaration: Android 10+ with Vulkan 1.2, arm64, touch screen. Actual supported GPU list is not established.
The APK contains content JSON and six compiled SPIR-V shaders (the HUD shaders are the UI assets; HUD glyphs are code-generated). At boot the host checks every required asset through AAssetManager, then mirrors them into <cacheDir>/konbini/package. Save / replay / settings / diagnostics live under <filesDir>/konbini, the regenerable geometry cache under <cacheDir>/konbini/geometry.
The selected mobile graphics profile (MobileHigh / MobileLow) is logged and written to <filesDir>/konbini/diagnostics/graphics-profile.log.
assembleDebug fails when the APK lacks a required asset or libkonbini_mobile.so, or carries another ABI.
CMake stages assets before the Gradle merge-assets task. Rebuild after changing content or shaders.
Release signing is not configured. No Play Store publication was performed.

## Xcode / iOS
Rendering policy (neco, 2026-10-03): iOS draws with Metal directly. The renderer is still Pictor, through its Metal backend (pictor::MetalContext on the UIView's CAMetalLayer). The game domain never calls Metal itself. Android stays on Vulkan.
Required: a Mac, Xcode with the iOS SDK (Metal is part of the SDK), CMake >=3.28, and the Pictor revision whose Metal backend covers the features KonbiniDominant draws. No MoltenVK library, Vulkan headers, or Vulkan loader is needed for iOS.
There is no working iOS build procedure yet. It is blocked on the upstream prerequisites listed in KD-MOB-006 (spec/tasks/2026-07-31-kd-mob-006-ios-package-integration.md): Pictor Metal backend coverage, iOS Metal in the Ergo render contract, and Pictor's iOS Metal validation. KD-MOB-006 writes the configure / Xcode steps once those land.
The intended shape: configure with the Xcode generator (-DCMAKE_SYSTEM_NAME=iOS, iphoneos or iphonesimulator SDK in separate build directories, arm64, -DKONBINI_BUILD_TESTS=OFF), inject the signing team from outside the repository (-DKONBINI_APPLE_TEAM), open the generated project and build konbini_mobile. The bundle carries content JSON and the Metal shader library; how Metal shaders are produced is TBD-IOS-METAL-SHADER-01.
The application uses UIKit lifecycle, CAMetalLayer, and landscape-only orientation.
Legacy (MoltenVK policy, until 2026-10-02): the iOS branch of cmake/MobileVulkan.cmake (KONBINI_MOLTENVK_LIBRARY / KONBINI_VULKAN_HEADERS), mobile/ios/vulkan_portability.cpp and the vkCreateInstance / vkCreateDevice renames on Pictor in mobile/CMakeLists.txt still exist in the code. They are not a supported iOS route and are removed by KD-MOB-006.
No IPA, signing, simulator run, or physical-device run has been performed.

## Outstanding verification
- Android Studio IDE sync (command-line Gradle build, NDK compile/link and APK asset inspection were done in KD-MOB-005).
- Xcode compile/link, signing, bundle resources, Metal device/simulator behaviour (after the KD-MOB-006 upstream prerequisites).
- Native startup, actual picture, landscape rotation, safe-area/notch/navigation-bar layout.
- Tap/build/skill selection, drag/pinch, cancellation and background/foreground recovery on both platforms.
- Phase 1–4 clear and sustained performance / memory behavior on actual phones.
- Small-screen HUD readability and thermal load. Desktop compilation does not verify these.

## Sources
Android native Vulkan: https://developer.android.com/ndk/guides/graphics/
Android Gradle plugin: https://developer.android.com/build/releases/about-agp
Apple Metal: https://developer.apple.com/documentation/metal
CAMetalLayer: https://developer.apple.com/documentation/quartzcore/cametallayer
Apple supported orientations: https://developer.apple.com/documentation/bundleresources/information-property-list/uisupportedinterfaceorientations
