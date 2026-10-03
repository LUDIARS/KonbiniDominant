# Mobile development setup

## Status

REQ-PLATFORM-01のtoolchain / packaging contract。
KonbiniDominant の Android / iOS host と package target は実装済みだが、
build、install、device launch は未検証。実装された手順と残る確認項目は
[mobile-native](mobile-native.md)を正本とする。

Windows first playableの
[native setup](native-development.md)を置き換えず、追加platformとして扱う。

## Common requirements

- C++20 / CMake
- exact revisionを検証したPictor / Ergo / Figmentum
- host環境でSPIR-Vを生成または検証するshader build
- packaged read-only assetとplatform writable pathの分離
- ARM64でx86 / Win32専用compile optionを有効にしない
- mobile hostだけがOS SDK header / lifecycle callbackを所有する

`konbini_sim`とFigmentum semantic planはdesktop / mobileで同じtargetを使う。
platformごとにsource copyを作らない。

## Android

必要な構成:

- Android SDK / NDK
- CMake Android toolchain
- Gradle projectとmanifest
- thin native host
- `ANativeWindow`を受けるPictor `AndroidSurfaceProvider`
- packaged assetを読むasset manager adapter
- writable files / cache directory adapter

最初のABIは`arm64-v8a`。最低Android API、端末tier、追加ABIは
`TBD-MOBILE-OS-01`で確定する。

native hostは次をforwardする。

- create / resume / pause / destroy
- native window created / resized / destroyed
- touch contact
- display density / safe area相当のinsets
- memory pressure
- thermal state

Pictor mobile buildはNDK Vulkanを利用し、GLFWを要求しない。
Ergo render targetがdesktop `find_package(Vulkan)`によりno-renderへ縮退しない
ことをconfigure時に検証する。

### Installed Android toolchain (KD-MOB-005, 2026-10-02)

KD-MOB-005で開発機へ導入・使用した版。システム環境変数 (setx / レジストリ) は
変更せず、command単位で`JAVA_HOME` / `ANDROID_HOME` / `ANDROID_SDK_ROOT`を
指定する。

| 項目 | 版 | 場所 |
|---|---|---|
| Android SDK root | — | `E:/Android/Sdk` (`mobile/android/local.properties`の`sdk.dir`。gitignore対象) |
| NDK (Side by side) | 27.2.12479018 (r27c) | `E:/Android/Sdk/ndk/27.2.12479018` |
| SDK CMake | 3.31.1 | `E:/Android/Sdk/cmake/3.31.1` |
| SDK Platform | android-35 | `E:/Android/Sdk/platforms/android-35` |
| Build-Tools | 35.0.0 | `E:/Android/Sdk/build-tools/35.0.0` |
| JDK | 17.0.8.101 (hotspot) | `C:/Program Files (x86)/Android/openjdk/jdk-17.0.8.101-hotspot` |
| sdkmanager | cmdline-tools 11.0 | `C:/Program Files (x86)/Android/android-sdk/cmdline-tools/11.0/bin` |
| Gradle | 8.11.1 (wrapper、`distributionSha256Sum`固定) | `mobile/android/gradlew(.bat)`, `mobile/android/gradle/wrapper/` |
| AGP | 8.9.2 | `mobile/android/build.gradle` |
| host glslc | Vulkan SDK 1.4.341.1 | `VULKAN_SDK` (`C:/VulkanSDK/1.4.341.1`) |

導入command:

```text
JAVA_HOME=<JDK 17> sdkmanager --sdk_root=E:/Android/Sdk --licenses
JAVA_HOME=<JDK 17> sdkmanager --sdk_root=E:/Android/Sdk "ndk;27.2.12479018" "cmake;3.31.1" "platforms;android-35" "build-tools;35.0.0"
```

package build (`mobile/android`):

```text
set JAVA_HOME=<JDK 17>
set ANDROID_HOME=E:\Android\Sdk
gradlew.bat --no-daemon :app:assembleDebug
```

`assembleDebug`は`verifyKonbiniDebugApk`で終わる。APKに
`mobile/android/required-assets.txt`の全asset、`lib/arm64-v8a/libkonbini_mobile.so`
が無い、またはarm64-v8a以外のABIがある場合はbuild失敗にする。
`mergeDebugAssets`前の`verifyKonbiniDebugStagedAssets`はCMakeがstageした
`build-mobile-assets/debug`を同じlistで検査する。

### NDK libc++ compatibility

NDK r27のlibc++ 18は浮動小数点`std::from_chars`を持たない (libc++ 20で追加)。
KD-MOB-005ではpinned Figmentum (`src/garment/profile.cpp`) とPictor
(`src/visus/visus_json.cpp`) がこれを使っていたため、Android buildだけ
`mobile/android/compat/libcxx_float_from_chars.h`をforce-includeしていた。
KD-MAC-001でPictor #2309 / Figmentum #2308 (どちらもlocale非依存の自前parseへ
置換) にpinを上げ、KDがbuildする上流targetから浮動小数点`std::from_chars`が
消えたのでshimと`-include`を削除した。KD自身のJSON parserは
`__cpp_lib_to_chars`が無い環境でclassic locale streamを使い、整数の
`std::from_chars`だけを直接呼ぶ。Apple libc++も同じ制約を持つ
([macOS development](macos-development.md))。

## iOS

必要な構成:

- macOS build host、Xcode、iOS SDK
- CMake iOS toolchainまたはXcode統合
- Objective-C++のthin host
- `UIView` / `CAMetalLayer`
- Metal / QuartzCore framework (iOS SDK同梱。MoltenVK / Vulkan loaderは使わない)
- KDの描画機能をカバーしたPictor Metal backendのpin
  ([KD-MOB-006の前提](../tasks/2026-07-31-kd-mob-006-ios-package-integration.md#前提-上流タスク))
- bundle asset reader
- Application Support / Caches等のwritable path adapter
- signing / bundle identifierの外部設定

iOSはMetalで直接描画する (neco決定 2026-10-03。Androidは変えない)。
`CAMetalLayer`をPictor `MetalContext`へ渡し、game domainからMetalを直接扱わない。
Metal device / feature capabilityはconfigureだけでなくruntimeにも検査し、
不足を通常surface失敗へ畳まない。

旧方針 (〜2026-10-02) はPictor `IOSSurfaceProvider` + MoltenVKで、portability
extension / capabilityを検査していた。その資産はKD-MOB-006の実装で撤去する。

minimum iOS version、device tier、signing ownerは`TBD-MOBILE-OS-01`で確定する。
secretや個人team IDをrepositoryへcommitしない。

## Vulkan and shader split

desktopの単一`find_package(Vulkan REQUIRED COMPONENTS glslc)`をmobileへ
そのまま適用しない。iOSはVulkanを使わない。

```text
HostShaderTools
  glslc or reviewed precompiled SPIR-V

RuntimeGraphics
  Windows: Vulkan::Vulkan
  Android: NDK vulkan
  iOS: Metal (Pictor MetalContext。Vulkanを要求しない)
```

game側targetはplatformごとのruntime graphics targetをlinkし、shader生成targetは
host toolだけを使う。iOSのMetal shader library生成は`TBD-IOS-METAL-SHADER-01`。必須shaderがpackageに無ければ起動をfail-fastする。

## Assets

```text
tracked source
  data/content/
  shaders/
  data/ui/

package read-only
  validated content
  SPIR-V
  UI assets
  optional prebaked low-LOD geometry

runtime writable
  save/
  replay/
  settings/
  cache/figmentum/
  diagnostics/
```

build machineの絶対pathやrepository layoutをpackage内で参照しない。

## Build target separation

実装 target は少なくとも次の責務を分ける。

- desktop native app
- Android native library / package
- iOS native library / app
- headless simulation
- tests
- host shader compilation

unsupported toolchainを検出した場合、別platform executableやheadless targetへ
silent fallbackしない。

## Verification boundary

implementation PRではconfigure / package生成結果を記録する。
install / startup / touch / background-resume / thermal / performanceは
actual-device validation taskへ分離する。

起動確認を行う時はCc policyに従い、プロジェクト本体folder、
Concordia testing claim / release、Excubitor、TestWorkflow証跡を使う。
worktreeや直接実行でのstartup確認は禁止する。
