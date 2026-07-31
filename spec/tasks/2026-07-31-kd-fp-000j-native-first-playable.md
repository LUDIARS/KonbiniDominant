---
task: kd-fp-000j-native-first-playable
project: KonbiniDominant
kind: 実装
status: pending
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
