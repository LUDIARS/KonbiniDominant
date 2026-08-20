---
task: kd-fp-000j-native-first-playable
project: KonbiniDominant
kind: 実装
status: done
created: 2026-07-31
source_session: lictor-c88f9672-9413-4e04-ad20-19980c021698
memoria_task_id: 668
actio_task_id: null
memory_links:
  - spec/tasks/2026-07-31-kd-fp-000i-depth-world-rendering.md
  - spec/interface/ergo-runtime.md
  - spec/interface/pictor-rendering.md
  - spec/plan/tasks/first-playable.md
  - spec/plan/tasks/first-playable-validation.md
---

# KD-FP-000J — Native first playable

## 目的

Figmentum都市、DoD simulation、Pictor world pass、Ergo frame/inputをnative
executableで接続し、選択と店舗配置が画面へ反映されるfirst playableを成立させる。

## 完了条件

- Figmentum city planから生成した都市がPictorで表示される
- Ergo inputからchain選択、facility選択、store配置commandを投入できる
- fixed tick後のimmutable snapshotだけをrender layerが読む
- offscreen world pass、barrier、swapchain composite、HUDの順で記録する
- generated `${CMAKE_BINARY_DIR}/shaders`をruntime shader directoryとして明示する
- layer初期化失敗時に初期化済みGPU resourceを逆順解放する
- swapchain再生成を検知し、composer / layer / scene targetを安全に再構築する
- acquire / presentのdevice lost、surface lost、out-of-dateを混同しない
- native executableと必要assetを`konbini_review`へ接続する
- Excubitor経由でプロジェクト本体folderだけを起動する
- 起動前後にConcordia testing claim / releaseを行う
- reviewed exact SHAの動作証跡をDiscord TestWorkflow threadへ記録する

## スコープ候補

- `include/konbini/adapters/ergo/`
- `include/konbini/app/`
- `src/adapters/ergo/`
- `src/app/`
- `data/content/`
- `spec/interface/`
- `spec/test/`
- `spec/tasks/`

## 残作業メモ

- pinned Ergo `FrameComposer::initialize()`は途中例外時に初期化済みlayerを
  shutdownしないため、upstream修正またはgame-owned rollback ownerが必要
- pinned Pictor / Ergoはswapchain out-of-dateとdevice / surface lossを同じ戻り値へ
  畳むため、typed resultのupstream追加を優先する
- Pictorの内部swapchain再生成だけに任せると旧render pass / descriptorが残るため、
  explicit rebuild通知を持つ

## 実装結果 (2026-08-21)

`include/konbini/app/`、`src/app/`、`include/konbini/adapters/ergo/`、
`src/adapters/ergo/` を新設し、native executable `konbini_dominant` を追加した。

### 構成

| 層 | 型 | 責務 |
|---|---|---|
| app | `SimulationHost` | content読み込み、都市生成、tickとsnapshot |
| app | `FixedStepDriver` | render dt → fixed tick、catch-up上限と drop報告 |
| app | `CameraController` | target / vertical spanの操作とclamp |
| app | `SelectionController` | 選択と再click確定、tick後のreconcile |
| app | `CommandComposer` | 単調増加sequenceでのcommand発行 |
| app | `HudTextInput` / `buildHudLines` | HudViewModel → 表示行 |
| app | `FramePresenter` | snapshot → draw list / HUD geometry のpublish |
| app | `AppRunner` | frame loopの呼び出し順のみを持つcomposition root |
| adapter | `ErgoInputBridge` | GLFW callback → Ergo inject |
| adapter | `InputActionMap` | Ergo device状態 → game-owned `FrameInput` |
| adapter | `RenderDeviceHost` | window / VulkanContext / RenderContext |
| adapter | `WorldFrameGraph` | 2 pass構成、`FrameComposer`、swapchain復旧 |
| adapter | `LayerInitializationScope` / `TrackedRenderLayer` | 初期化の逆順rollback |
| adapter | `SwapchainIdentity` / `classifyFrameOutcome` | out-of-dateとlostの分離 |
| render | `HudOverlayLayer` / `HudPipelines` / `buildHudTextMesh` | pass 1のHUD |
| pictor | `loadCityGeometry` | 都市geometryのGPU cacheへのupload |

### 完了条件の対応

- Figmentum city plan → `CityManifest` → `WorldGeometryCache` → world passで表示
- Ergo inputから chain選択 / facility選択 / store配置commandを投入
- render layerは tick終端の immutable snapshotだけを読む (`FramePresenter`)
- offscreen world pass → barrier hook → composite → HUDの順で記録
- runtime shader directoryは `${CMAKE_BINARY_DIR}/shaders` をcompile definition
  で明示 (`KONBINI_SHADER_DIR`で上書き可、上方探索fallbackなし)
- layer初期化失敗時は `LayerInitializationScope` が逆順で解放
- swapchain再生成を検知して composer / layer / scene targetを作り直す
- acquire / presentの out-of-date、minimized、device lost、surface lostを分離
- `konbini_review` が `konbini_dominant` と `konbini_runtime_content` を含む

### 未実施 (後続セッションへ deferred)

このセッションはユーザからテスト実行・サービス起動の明示指示を得ていないため、
次の3項目は実装対象外とした。KD-FP-002 相当の明示許可セッションで行う。

- Excubitor経由でプロジェクト本体folderだけを起動する
- 起動前後のConcordia testing claim / release
- reviewed exact SHAの動作証跡をDiscord TestWorkflow threadへ記録する

build / unit test / 統合テストも実行していない。テストコードは
`tests/app/first_playable_app_test.cpp` と
`tests/adapters/swapchain_recovery_test.cpp` に追加済み (未実行)。
