---
task: kd-mob-001-ergo-platform-render-contract
project: KonbiniDominant
kind: 実装
status: review
created: 2026-07-31
source_session: lictor-c88f9672-9413-4e04-ad20-19980c021698
memoria_task_id: 671
actio_task_id: b6cdfa30-535a-4b85-92f6-f10fec65baef
actio_followup_task_id: 4e5ae4e8-2847-4142-a1d8-9cd62a58066a
memory_links:
  - spec/tasks/2026-07-31-kd-mob-000-smartphone-contract.md
  - spec/interface/mobile-platform.md
  - spec/interface/ergo-runtime.md
  - spec/setup/mobile-development.md
---

# KD-MOB-001 — Ergo mobile dependency integration

## 目的

platform-neutral render contractを持つreview済みErgo revisionを
KonbiniDominantへ固定し、Windows / Android / iOS hostが同じconsumer境界を
利用できるようにする。

## Upstream prerequisite

Ergo側の実装は、着手時に`LUDIARS/Ergo/spec/tasks/`へtask-workflow 2.1形式で
保存し、Ergo専用branch / PRで行う。必要なupstream contract:

- `RenderContext::surface`が`pictor::ISurfaceProvider*`をborrowする
- Ergo公開render headerが`GlfwSurfaceProvider`具象型を要求しない
- desktopでは既存GLFW providerを同じinterface経由で利用する
- Android / iOSでPictor targetが提供するruntime Vulkan contractを認識する
- mobile configure時にdesktop `Vulkan::Vulkan`不在だけでrenderを無効化しない
- real render不可をVulkan-free成功へsilent fallbackしない
- FrameComposer / layerの所有・shutdown順を変更しない
- cross-platform build契約をErgo specへ記録する

## 完了条件

- upstream Ergo PRがreview済みでmergeされている
- merge revisionをexactかつclean source検証付きで固定する
- Konbiniのdesktop surface adapterが`ISurfaceProvider`境界でbuild可能
- Android / iOS configureがreal Ergo render pathを選択できる
- required render capability不在をheadless成功へsilent fallbackしない
- dependency revisionとconsumer contractをKonbini specへ反映する

## スコープ

- KonbiniDominant dependency pin / exact-source検証
- KonbiniDominant Ergo consumer adapter
- KonbiniDominant CMake / interface spec

Ergo repositoryの編集はこのtaskへ含めない。KonbiniDominantへErgo sourceを
copyしない。touch、native host、app packagingは後続taskへ分離する。

## Delivery

- KonbiniDominant専用branch / worktreeで実装する
- commit / push / KonbiniDominant PRを作成して停止する
- upstream未mergeならconsumer側へ仮実装せずblockedとして報告する
- unit / integration / behavior / startup testは明示指示なしに実行しない

## 実装結果 (2026-10-01, 2026-10-02更新)

### Dependency revision

| dependency | 旧pin | 新pin | 理由 |
|---|---|---|---|
| Ergo | `771b027f0e5492015b27f54c3bab1fd5c1ae4790` | `7f0d6bbd34dced4fc6664a5f04bce9910e893537` | upstream render contract (`b618de7a`) + MSVC compile修正 (Ergo #2240) |
| Pictor | `c088e8d1b7b9e2625b7a8d923c89d4d684566c16` | `c6b1c7538ad00623221cea041e525342374f6126` | 旧pinは履歴書き換えでremote取得不可。同じmerge (#105) の書き換え後commitで、差分はcomment / docsの伏せ字のみ |

Ergo `7f0d6bbd`は新しいPictor APIを要求しない (`ISurfaceProvider`、
`NativeWindowHandle::Type`、`PICTOR_HAS_VULKAN`は既存)。exact HEAD + clean
worktree検証は既存の`konbini_verify_exact_git_source()`をそのまま使う。

### Consumer contract

- `RenderDeviceHost`はdesktop / mobileとも`RenderContext::surface`へ
  `ISurfaceProvider`を借用で渡す (mobileのnull surfaceを解消)
- `requireRenderReady()`がbackend contractとcontext前提を検査し、
  `RenderUnavailableError`で起動を止める
- `WorldFrameGraph`は`FrameComposer::initialize()`の`RenderBackendError`を
  捨てず、`None`以外はcomposerをshutdownして明示errorにする
- `ERGO_RENDER_REQUIRE_REAL=ON`と`cmake/RequireErgoRealRender.cmake`で、
  Vulkan-free / 期待外platformの`ergo_render`をconfigure errorにする。
  Android / iOSは`ANDROID` / `IOS` platform定義を要求する

詳細は[Ergo runtime contract](../interface/ergo-runtime.md#render-readiness)。

### Tests

- `konbini_render_readiness_tests` — backend contractとcontext前提の型付き失敗
- `konbini_ergo_real_render_contract_tests` — desktop / Android / iOSの
  configure契約 (script mode、toolchain不要)

### Upstream fix

2026-10-01時点のpin `b618de7a`は`cmake/ErgoRenderBackend.cmake`で
`ERGO_RENDER_VULKAN_SOURCE`を手動エスケープしており、
`target_compile_definitions()`の自動エスケープと二重になって、
Windows x64 / Visual Studio 17 2022でreal render有効時に
`src/render/render_backend.cpp`が`C2017` / `C2001`でcompileできなかった。
consumer側で定義を書き換えるのは仮実装に当たるため行わず、blockedとした。

Ergo #2240 (「ERGO_RENDER_VULKAN_SOURCE の定義値を二重にエスケープしない」) が
Ergo mainへ`7f0d6bbd34dced4fc6664a5f04bce9910e893537`としてmergeされたので、
`KONBINI_ERGO_REVISION`をこのrevisionへ更新した。`b618de7a..7f0d6bbd`の差分は
`cmake/ErgoRenderBackend.cmake`のみで、consumer contractは変わらない。
exact HEAD + clean worktree検証は`konbini_verify_exact_git_source()`が
新しいrevisionに対して行う。

### Verification (2026-10-02)

- Windows x64 / Visual Studio 17 2022 / Debug、`KONBINI_BUILD_TESTS=ON`で
  build directoryを削除してからconfigure (`ergo_render: real render path
  enabled (platform=desktop)`) とbuildを行い、errorなしで完了した
- fetch済みErgo copyはHEAD `7f0d6bbd` / `git status`空で、一時patchは使っていない
- `ctest -C Debug` 16件すべて成功
- Android / iOS実configure、app起動、実機検証は未実施 (後続KD-MOB-005〜007)
