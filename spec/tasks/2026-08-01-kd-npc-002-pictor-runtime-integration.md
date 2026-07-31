---
task: kd-npc-002-pictor-runtime-integration
project: KonbiniDominant
kind: 実装
status: pending
created: 2026-08-01
source_session: lictor-c88f9672-9413-4e04-ad20-19980c021698
memoria_task_id: 683
actio_task_id: null
memory_links:
  - spec/tasks/2026-08-01-kd-npc-001-visia-presentation.md
  - spec/interface/visia-presentation.md
  - spec/interface/pictor-rendering.md
  - spec/plan/implementation-roadmap.md
---

# KD-NPC-002 — Pictor / Ergo NPC runtime integration

## 目的

KD-NPC-001のresident、speech、placement cue、Visia geometryをproduction
Pictor / Ergo frameへ接続し、native app上で表示・更新・破棄する。

## 前提

- Gate 5の`GpuAssetStore`、`IBatchGpuSource`、`PictorFrameBridge`がreview済み
- native appがimmutable `RenderSnapshot`をframeごとにconsumeできる
- Pictor text atlasへ同梱できるfontとライセンスが確定している

前提未達の間にKonbini側へ仮のPictor実装や偽のsuccess pathを追加しない。

## 完了条件

- resident stable IDとPictor object lifecycleを同期する
- ZOC再割当で目的地が変わるresidentを直前frame poseから補間し、瞬間移動を見せない
- Visia resident meshを1回uploadし、resident instance間で共有する
- world head anchorをscreenへ投影し、距離・off-screen・同時表示上限でbubbleをcullする
- 日本語を含む有限の会話lineを共有text atlasから描画する
- placement cueから店舗pose overrideとlanding effect lifecycleを開始する
- cue取りこぼし時も店舗をfinal poseで表示する
- removed resident / expired effectのPictor objectとresource参照を解放する
- desktopとmobileの同じsnapshot contractで動作する
- Excubitor経由の起動確認とTestWorkflow証跡を残す

## スコープ (編集可ディレクトリ)

- KonbiniDominant Pictor / Ergo adapter
- resident / effect object sync
- speech atlas / overlay layer
- native frame integration

Figmentum pedestrian path schemaはKD-NPC-003へ分離する。gameplayへ影響する
店舗評価とauthoritative個人AIは要件確定前のfuture candidateであり、このtaskへ
暗黙に含めない。
