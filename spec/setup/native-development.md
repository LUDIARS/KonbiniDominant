# Native development setup

## Status

first playable のsetup contract。実装済みdependencyはexact revisionをCMakeで
検証し、未実装dependencyは導入するstageで同じ方式へ同期する。

## Required

- Windows + MSVC 2022を第一対象
- CMake
- C++20 compiler
- Vulkan SDK
- `glslc`
- Git
- pinned `FetchContent` または検証付きsource overrideで解決した
  Pictor / Ergo / Figmentum source

required toolが無ければconfigureでfail-fastする。headless simulation targetだけは
Vulkan非依存でbuildできる設計にする。

## Dependency revisions

| dependency | inspected `origin/main` |
|---|---|
| Pictor | `c088e8d1b7b9e2625b7a8d923c89d4d684566c16` |
| Ergo | `771b027f0e5492015b27f54c3bab1fd5c1ae4790` |
| Figmentum | `3ee998f487d984f54003c4ec3c4f7ba00b53eec3` |

floating `main` は使わない。Figmentum は `FetchContent` でpopulateした後、
exact HEADとclean worktreeを検証してからdependency CMakeを評価する。
Pictor / Ergoも導入stageで同じfail-fast契約に従う。

## CMake order

Pictor targetの有無をErgo configure時に見るため、順序は固定する。

```cmake
set(PICTOR_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(PICTOR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${KONBINI_PICTOR_DIR}" "${CMAKE_BINARY_DIR}/_pictor")

# 必要なERGO_BUILD_*だけON、testはhost側方針に合わせる
add_subdirectory("${KONBINI_ERGO_DIR}" "${CMAKE_BINARY_DIR}/_ergo")

# Figmentumは未検証sourceのCMakeを実行しない
FetchContent_Populate(figmentum)
konbini_verify_exact_git_source(
  "${figmentum_SOURCE_DIR}"
  "3ee998f487d984f54003c4ec3c4f7ba00b53eec3"
)
add_subdirectory(
  "${figmentum_SOURCE_DIR}"
  "${figmentum_BINARY_DIR}"
  EXCLUDE_FROM_ALL
)
```

pathを個人絶対pathへhard-codeしない。CMake cache / presetで解決し、
既定を置く場合もrepository相対の存在確認と明示errorを持つ。

## Targets

予定:

- `konbini_sim` — headless DoD core
- `konbini_city` — manifest / recipe domain interface
- `konbini_app` — Ergo / Pictor / Figmentum adaptersを含むnative executable
- `konbini_tests`群 — 責務別

`konbini_sim`からPictor / Vulkan / Figmentumへlinkしない。

現在repositoryには`konbini_sim`、`konbini_city`、
`konbini_figmentum_adapter`、`konbini_render_domain`、
`konbini_pictor_scene_targets`、`konbini_world_composite`、
`konbini_sim_tests`が存在する。`konbini_app`はKD-FP-001の後続stageで追加する。

## Options

| option | 既定 | 意味 |
|---|---|---|
| `KONBINI_BUILD_FIGMENTUM_ADAPTER` | `ON` | 固定revisionのFigmentum city adapterをbuild |
| `KONBINI_BUILD_RENDER` | `ON` | 固定revisionのPictor / ErgoとVulkan GPU境界をbuild |
| `KONBINI_BUILD_TESTS` | `OFF` | headless testのbuildと`ctest`登録 |

## Compiler

- MSVC consumer targetへ `/utf-8`
- warningを有効化
- source / specはUTF-8、repository改行はLF
- sanitizer対応compilerではheadless testへ適用

## Assets / generated data

```text
data/content/     tracked
shaders/          tracked source
data/ui/          tracked
data/generated/   ignored; Figmentum mesh/cache
data/cache/       ignored
```

generated meshはsaveの正本ではない。削除してもseed/recipeから再生成できる。

## Configuration

必須候補:

- dependency paths
- shader directory
- content root
- generated cache root
- pipeline profile
- explicit audio backend
- debug placeholder opt-in

個人環境pathをsourceへ書かず、非secret既定はversioned configに置く。
本作は現時点でnetwork secretを必要としない。

## Future commands

実装後の形:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DKONBINI_BUILD_FIGMENTUM_ADAPTER=OFF -DKONBINI_BUILD_RENDER=OFF `
  -DKONBINI_BUILD_TESTS=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

上のcommandはFigmentum adapterを明示的に無効化したheadless build。
既定構成はGitで固定revisionのFigmentum / Pictor / Ergoを取得し、各sourceの
exact HEADとclean worktreeを検証してからdependency CMakeを評価する。
Vulkan非依存のheadless構成は`KONBINI_BUILD_RENDER=OFF`で明示する。

## Runtime

- 実行はrepository本体folderのCc/Excubitor方針に従う
- missing Vulkan / shader / input backendをsilent headlessへ落とさない
- headless実行は専用target / 明示optionで選ぶ
