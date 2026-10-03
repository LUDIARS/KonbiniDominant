---
task: kd-mob-002-pictor-surface-recovery
project: KonbiniDominant
kind: 実装
status: in_review
created: 2026-07-31
source_session: lictor-c88f9672-9413-4e04-ad20-19980c021698
memoria_task_id: 672
actio_task_id: "actio:05c24178-686b-4152-9d7d-00bfd68fc908"
memory_links:
  - spec/tasks/2026-07-31-kd-mob-000-smartphone-contract.md
  - spec/interface/mobile-platform.md
  - spec/interface/pictor-rendering.md
  - spec/setup/mobile-development.md
value_ids:
  - UX-KD-W4
---

# KD-MOB-002 — Pictor mobile recovery integration

## 目的

型付きmobile surface / device recovery contractを持つreview済みPictor revisionを
KonbiniDominantへ固定し、Android / iOSでsimulationを失わず描画を復旧できる
consumer境界へ接続する。

> **iOS は Metal へ変更 (2026-10-03)**: neco決定によりiOSはMoltenVKを使わずPictor
> Metal backendでMetal直描画する ([mobile-platform](../interface/mobile-platform.md#ios描画方針-2026-10-03-決定)、
> [KD-MOB-006](2026-07-31-kd-mob-006-ios-package-integration.md#前提-上流タスク))。
> 本taskのiOS記述 (MoltenVK portability検査、`vulkan_portability.cpp` shim) は旧方針。shimの撤去とMetal経路はKD-MOB-006で扱う。Android / desktopの内容と完了済みの実装結果は変えない。

## Upstream prerequisite

Pictor側の実装は、着手時に`LUDIARS/Pictor/spec/tasks/`へtask-workflow 2.1形式で
保存し、Pictor専用branch / PRで行う。必要なupstream contract:

- acquire / present結果が少なくとも`Ready`、`RecreateSwapchain`、
  `SurfaceLost`、`DeviceLost`を区別する
- Android native window消失中はsurface / swapchain作成を試みない
- iOS MoltenVK portability extension / featureをcapability検査する
- required extension欠落を通常resizeやframe skipへ畳まない
- pause / suspend / surface lost中はGPU submissionを抑止する
- surface復帰時のswapchain依存resource再構築順をpublic contractへ記録する
- Android / iOS providerのownershipを変更せず、host所有を維持する
- desktop surface pathを同じtyped resultへ移行する

## 完了条件

- upstream Pictor PRがreview済みでmergeされている
- merge revisionをexactかつclean source検証付きで固定する
- typed resultをapp lifecycle / render resource ownerへ漏れなくmappingする
- `SurfaceLost`でsimulation stateを保持しGPU submissionを止める
- `RecreateSwapchain`でreplacement-before-retire順を守る
- `DeviceLost`を通常resizeとしてsilent retryしない
- dependency revisionとconsumer contractをKonbini specへ反映する

## スコープ

- KonbiniDominant dependency pin / exact-source検証
- KonbiniDominant Pictor lifecycle adapter
- KonbiniDominant CMake / interface spec

Pictor repositoryの編集はこのtaskへ含めない。Konbini固有world draw、touch、
APK / iOS appは後続taskへ分離する。

## Delivery

- KonbiniDominant専用branch / worktreeで実装する
- commit / push / KonbiniDominant PRを作成して停止する
- upstream未mergeならconsumer側へ仮実装せずblockedとして報告する
- unit / integration / behavior / startup testは明示指示なしに実行しない

## 実装結果 (2026-10-02)

Actio: `actio:05c24178-686b-4152-9d7d-00bfd68fc908`。価値ID: `UX-KD-W4`
(Windows / Android / iOSで同じsimulationを使い、platform host以外へOS APIを
漏らさない)。

### Upstream

Pictor #2243がmain
`02ea861c1657f1f7f3b4d41c361388e7646cbe47`
(feat(surface): typed mobile surface / device recovery contract) でmerge済み。
`KONBINI_PICTOR_REVISION`をこのrevisionへ更新した。Ergoは
`7f0d6bbd34dced4fc6664a5f04bce9910e893537`のまま。Ergoは新Pictorでも
変更なしでbuildできる (legacyの`acquire_next_image()` / `present()`は
typed APIのwrapperとして残っている)。Pictor / Ergo / Figmentumは編集していない。

### 受け入れ条件

- C-1 konbini_verify_exact_git_source(pictor): `KONBINI_PICTOR_REVISION`が`02ea861c`で、exact HEAD + clean worktree検証を通る。pinとsetup spec / READMEの表は`konbini_dependency_pin_contract_tests`で一致を検査する
- C-2 classifyFrameResult(result, presented): Pictorの全`FrameStatus`を1つの`FrameOutcome`へ対応付け、loss系は`presented`より優先する
- C-3 WorldFrameGraph::runFrame(): `gateFrame()`が`Ready`以外ならacquire / submit / present / swapchain再生成を行わず、その結果を返す
- C-4 RenderLifecycle::observe(SurfaceLost): `ReinitializeRequired`になりGPU submissionを止める。resume / resize / 後続frameでは解除されず、simulation (`GameSession`) は破棄しない
- C-5 WorldFrameGraph::recreateSwapchainAndRebuild(): `RecreateSwapchain`でcomposer破棄 → Pictorの代替swapchain → 代替scene target作成後に旧target退役 → composer再構築の順を守る
- C-6 requiresReinitialize(DeviceLost): `DeviceLost`は再構築も通常resizeの再試行もせず、明示的な`reinitializeRender()`だけが復旧経路になる
- C-7 RenderInitError(result): `ContextInitStatus`を保持し、`SurfaceUnavailable`以外のextension / capability欠落は欠落名付きの構成errorになる
- C-8 spec: 対応表、lifecycle owner、再構築順、dependency revisionを`spec/interface/pictor-rendering.md#surface--device-recovery`ほかへ反映する

C-1は`tests/cmake/dependency_pin_contract_test.cmake`とconfigure時の
exact-source検証、C-2〜C-4 / C-6 / C-7は`tests/adapters/swapchain_recovery_test.cpp`
の単体テストで確認した。C-5の順序はコードと
spec上で確認した。実GPUが必要なため、headlessの単体テストでは検証していない。

### 変更した境界

- 追加: `FrameOutcome` / `classifyFrameResult()` (`frame_outcome.h`)、
  `RenderLifecycle` (`render_lifecycle.h`)、`RenderInitError`
  (`render_init_error.h`)
- `RenderDeviceHost`: `gateFrame()`、`setPresentationSuspended()`、
  初期化失敗を`RenderInitError`で返す
- `WorldFrameGraph`: frame前gate、typed `last_frame_result()`による分類、
  loss時は再構築しない
- `NativeMobileRuntime`: `RenderLifecycle`で抑止を管理し、
  `reinitializeRender()` / `renderState()` / `renderLossCause()`を追加した。
  lossでは例外を投げず`ReinitializeRequired`を公開する
- Android / iOS host: `ReinitializeRequired`なら明示的に`reinitializeRender()`
- desktop `AppRunner` / gallery: `requiresReinitialize`で終了 (exit code 2)
- 廃止: `SwapchainIdentity` / `classifyFrameOutcome()` / `probeDeviceLost()`
  (handle同一性とdevice idleからの推定)

### 検証

実施:

- Windows x64 / MSVC 19.39 / Visual Studio 17 2022、Debug、
  `KONBINI_BUILD_TESTS=ON`で新しいbuild directoryからconfigure + 全target build
  (render / Figmentum adapter有効)。Ergo `7f0d6bbd`はPictor `02ea861c`でbuild成功
- `ctest -C Debug`: 17/17 pass (`konbini_render_adapter_tests`、
  `konbini_dependency_pin_contract_tests`を含む)

未実施:

- desktop app / gallery / mobile appの起動 (指示により起動テストなし)
- Android NDK / iOS (Xcode + MoltenVK) build。mobile glue
  (`mobile/android/native_main.cpp`、`mobile/ios/main.mm`) はこのhostでは未compile
- 実機、実GPUでのsurface / device loss、最小化の動作確認 (KD-MOB-007)

### 残り

- `mobile/ios/vulkan_portability.cpp`のMoltenVK portability shimは、Pictor
  `02ea861c`が同じ拡張を自前で有効化するため冗長になった。shimは重複追加を
  避けるので共存はできる。撤去はiOS buildを検証できるKD-MOB-006で行う
- Ergo `FrameComposer::run_frame()`がtyped `FrameResult`を返せば、
  `last_frame_result()`の読み出しは不要になる (Ergo側の改善候補)
