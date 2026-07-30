# Native development setup

## Status

設計時点のsetup contract。実装開始時にexact version / commandをrepository実体へ
同期する。

## Required

- Windows + MSVC 2022を第一対象
- CMake
- C++20 compiler
- Vulkan SDK
- `glslc`
- Git
- siblingまたは明示pathで解決したPictor / Ergo / Figmentum source

required toolが無ければconfigureでfail-fastする。headless simulation targetだけは
Vulkan非依存でbuildできる設計にする。

## Dependency revisions at design time

| dependency | inspected `origin/main` |
|---|---|
| Pictor | `c088e8d1b7b9e2625b7a8d923c89d4d684566c16` |
| Ergo | `771b027f0e5492015b27f54c3bab1fd5c1ae4790` |
| Figmentum | `719af466c6f821fc2e518f652de162a8b9ebb3bd` |

実装branchではfloating `main`ではなく、submodule / lock metadata等の
再現可能な方法を選ぶ。方式は `TBD-DEPS-01`。

## CMake order

Pictor targetの有無をErgo configure時に見るため、順序は固定する。

```cmake
set(PICTOR_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(PICTOR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${KONBINI_PICTOR_DIR}" "${CMAKE_BINARY_DIR}/_pictor")

# 必要なERGO_BUILD_*だけON、testはhost側方針に合わせる
add_subdirectory("${KONBINI_ERGO_DIR}" "${CMAKE_BINARY_DIR}/_ergo")

# Figmentum CLI/testsをdefault buildへ巻き込まない
add_subdirectory(
  "${KONBINI_FIGMENTUM_DIR}"
  "${CMAKE_BINARY_DIR}/_figmentum"
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

現在repositoryに存在するのは`konbini_sim`と、その決定的primitiveを検証する
`konbini_sim_tests`だけ。`konbini_city` / `konbini_app`はKD-FP-001で追加する。

## Options

| option | 既定 | 意味 |
|---|---|---|
| `KONBINI_BUILD_TESTS` | `OFF` | headless testのbuildと`ctest`登録 |

## Compiler

- MSVC consumer targetへ `/utf-8`
- warningを有効化
- source / specはUTF-8、repository改行はLF
- sanitizer対応compilerではheadless testへ適用

## Assets / generated data

```text
data/content/     tracked
data/shaders/     tracked source
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
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DKONBINI_BUILD_TESTS=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

現時点ではVulkan / Pictor / Ergo / Figmentumへ依存しないため、上のcommandは
headless build専用として成立する。実際のoption / presetを追加したら、この文書を
同じPRで更新する。

## Runtime

- 実行はrepository本体folderのCc/Excubitor方針に従う
- missing Vulkan / shader / input backendをsilent headlessへ落とさない
- headless実行は専用target / 明示optionで選ぶ
