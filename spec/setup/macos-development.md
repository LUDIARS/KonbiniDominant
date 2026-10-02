# macOS development setup

## Status

KD-MAC-001 (2026-10-03)。macOS desktopでKonbiniDominantをconfigure / build /
`ctest`するhost手順の正本。Windowsの[native setup](native-development.md)を
置き換えず、追加のdesktop build hostとして扱う。アプリの起動・描画確認は
この手順の範囲外 (未実施)。

## Target

- macOS desktop、Apple Silicon (arm64)
- Apple clang + Apple libc++ (Command Line Tools)
- Vulkan loader + MoltenVK (Vulkan portability driver over Metal)
- generator: Ninja、configuration: Release

iOS packageは別手順 ([mobile-native](mobile-native.md#xcode--ios)) で、
Xcode本体とiOS SDKの選択が別途必要になる。このページのCommand Line Toolsだけの
hostではiOS buildはできない。

## Verified host

KD-MAC-001の検証機 (`neko@macat.local`)。

| 項目 | 版 |
|---|---|
| macOS | 26.6.2 (25G83) |
| CPU | Apple M1 (arm64) |
| Command Line Tools | 14.3.1 (`/Library/Developer/CommandLineTools`) |
| Apple clang | 14.0.3 (clang-1403.0.22.14.1) |
| macOS SDK | 13.3 |
| CMake | 4.4.2 (Homebrew) |
| Ninja | 1.13.2 (Homebrew) |
| Git | 2.39.2 (Apple Git-143) |

## Homebrew packages

```sh
brew install cmake ninja vulkan-headers vulkan-loader molten-vk shaderc vulkan-tools
```

| formula | 版 | 用途 |
|---|---|---|
| `vulkan-headers` | 1.4.357.0 | Vulkan header |
| `vulkan-loader` | 1.4.357.0 | `libvulkan` (`find_package(Vulkan)`) |
| `molten-vk` | 1.4.2 | Vulkan ICD (Metal backend) |
| `shaderc` | 2026.4 | host `glslc` (`find_package(Vulkan COMPONENTS glslc)`) |
| `vulkan-tools` | 1.4.357.0 | `vulkaninfo` |

Homebrewのprefixは`/opt/homebrew`。commandを実行するshellの`PATH`に
`/opt/homebrew/bin`を足す (ssh非対話shellでは既定で入らない)。

## Vulkan device check

```sh
export PATH=/opt/homebrew/bin:$PATH
vulkaninfo --summary
```

`deviceName = Apple M1`、`driverName = MoltenVK`、`apiVersion = 1.4.x`の
deviceが出ればloaderとMoltenVK ICDが揃っている。

## Dependency revisions

Pictor / Ergo / Figmentumは[native setup](native-development.md#dependency-revisions)
と同じ固定revisionをexact-source検証付きで取得する。Apple libc++は浮動小数点
`std::from_chars`を持たないため、Pictor #2309 (`502ba022`) と
Figmentum #2308 (`ff09a65d`) 以降が必要になる。

KD自身の制約:

- 浮動小数点`std::from_chars`を直接呼ばない。JSON数値は
  `__cpp_lib_to_chars`が無い環境でclassic locale streamへ落とす
  (`src/sim/content/json_document.cpp`)。整数の`std::from_chars`は使える
- `std::string`メンバーを持つ型に`operator<=>`をdefaultしない。Apple clang 14の
  libc++には`std::string`の三方比較が無く、defaulted `<=>`が暗黙に削除される。
  `std::map` keyは明示的な`operator<`を定義する
  (`FacilityGeometryCacheKey`、`include/konbini/city/facility_geometry.h`)

## Private Figmentum over SSH

Figmentumは非公開repositoryで、`FetchContent`は
`https://github.com/LUDIARS/Figmentum.git`を取得する。https認証を持たない
hostでは、configureするcommandの環境だけでGitのURL読み替えを付けてSSH鍵で
取得する。`git config --global`等の全体設定は変更しない。

```sh
GIT_CONFIG_COUNT=1 \
GIT_CONFIG_KEY_0=url.git@github.com:LUDIARS/.insteadOf \
GIT_CONFIG_VALUE_0=https://github.com/LUDIARS/ \
  cmake -S . -B build-macos ...
```

`GIT_CONFIG_COUNT` / `GIT_CONFIG_KEY_n` / `GIT_CONFIG_VALUE_n`はGit 2.31以降の
環境変数設定で、そのprocessと子process (FetchContentのgit) だけに効く。
一度populateした`build-macos/_deps`は以後のbuildで再取得しない。

## Configure / build / test

repository rootで実行する。

```sh
export PATH=/opt/homebrew/bin:$PATH
GIT_CONFIG_COUNT=1 \
GIT_CONFIG_KEY_0=url.git@github.com:LUDIARS/.insteadOf \
GIT_CONFIG_VALUE_0=https://github.com/LUDIARS/ \
  cmake -S . -B build-macos -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=/opt/homebrew \
    -DKONBINI_BUILD_TESTS=ON
cmake --build build-macos --parallel
ctest --test-dir build-macos --output-on-failure
```

既定構成 (`KONBINI_BUILD_RENDER=ON`、`KONBINI_BUILD_FIGMENTUM_ADAPTER=ON`) で
Pictor / Ergo / Figmentumを取得し、Ergoはdesktop real render path
(`platform=desktop`) を選ぶ。

## Verification record

KD-MAC-001 (2026-10-03)。作業branchはGitHubへpushできないため、
`git bundle create`したbranchを`scp`で送り、Macの
`~/LUDIARS/.kd-verify-macos`へcloneして上の手順を実行した。Mac上の本体clone
(`~/LUDIARS/KonbiniDominant`) には触れていない。

| host | configuration | build | ctest |
|---|---|---|---|
| Windows 11 x64 (MSVC, Visual Studio 17 2022) | Debug | 成功 | 34/34 passed |
| macOS 26.6.2 / Apple M1 (Apple clang 14.0.3, Ninja) | Release | 成功 (615/615) | 34/34 passed |
| Android arm64-v8a (NDK r27c, Gradle `:app:assembleDebug`) | Debug | 成功、`verifyKonbiniDebugApk`通過 | — |

macOS buildにはdependency sourceと既存KD sourceの警告 (`-Wswitch`、
`-Wmissing-field-initializers`等) が出るが、errorは無い。
アプリの起動、MoltenVK上の実描画、window / input確認は未実施。
