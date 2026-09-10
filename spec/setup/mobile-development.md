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

## iOS

必要な構成:

- macOS build host、Xcode、iOS SDK
- CMake iOS toolchainまたはXcode統合
- Objective-C++のthin host
- `UIView` / `CAMetalLayer`
- MoltenVK packageと明示link
- bundle asset reader
- Application Support / Caches等のwritable path adapter
- signing / bundle identifierの外部設定

Pictor `IOSSurfaceProvider`へ`CAMetalLayer`を渡し、game domainからMetalを
直接扱わない。MoltenVK portability extension / device capabilityは
configureだけでなくruntimeにも検査し、不足を通常surface失敗へ畳まない。

minimum iOS version、device tier、signing ownerは`TBD-MOBILE-OS-01`で確定する。
secretや個人team IDをrepositoryへcommitしない。

## Vulkan and shader split

desktopの単一`find_package(Vulkan REQUIRED COMPONENTS glslc)`をmobileへ
そのまま適用しない。

```text
HostShaderTools
  glslc or reviewed precompiled SPIR-V

RuntimeGraphics
  Windows: Vulkan::Vulkan
  Android: NDK vulkan
  iOS: MoltenVK
```

game側targetはplatformごとのruntime graphics targetをlinkし、shader生成targetは
host toolだけを使う。必須shaderがpackageに無ければ起動をfail-fastする。

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
